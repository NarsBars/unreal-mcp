#include "MCPServerRunnable.h"
#include "UnrealMCPBridge.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "JsonObjectConverter.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformTime.h"

// Buffer size for receiving data
namespace { constexpr int32 MCPBufferSize = 8192; }

FMCPServerRunnable::FMCPServerRunnable(UUnrealMCPBridge* InBridge, TSharedPtr<FSocket> InListenerSocket)
    : Bridge(InBridge)
    , ListenerSocket(InListenerSocket)
    , bRunning(true)
{
}

FMCPServerRunnable::~FMCPServerRunnable()
{
    // Note: We don't delete the sockets here as they're owned by the bridge
}

bool FMCPServerRunnable::Init()
{
    return true;
}

uint32 FMCPServerRunnable::Run()
{
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Server thread started on port 55557"));

    while (bRunning)
    {
        bool bPending = false;
        if (ListenerSocket->HasPendingConnection(bPending) && bPending)
        {
            ClientSocket = MakeShareable(ListenerSocket->Accept(TEXT("MCPClient")));
            if (ClientSocket.IsValid())
            {
                // Set socket options to improve connection stability
                ClientSocket->SetNoDelay(true);
                int32 SocketBufferSize = 65536;  // 64KB buffer
                ClientSocket->SetSendBufferSize(SocketBufferSize, SocketBufferSize);
                ClientSocket->SetReceiveBufferSize(SocketBufferSize, SocketBufferSize);

                uint8 Buffer[8192];
                while (bRunning)
                {
                    int32 BytesRead = 0;
                    if (ClientSocket->Recv(Buffer, sizeof(Buffer), BytesRead))
                    {
                        if (BytesRead == 0)
                        {
                            break;
                        }

                        // Convert received data to string
                        Buffer[BytesRead] = '\0';
                        FString ReceivedText = UTF8_TO_TCHAR(Buffer);

                        // Parse JSON
                        TSharedPtr<FJsonObject> JsonObject;
                        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedText);

                        if (FJsonSerializer::Deserialize(Reader, JsonObject))
                        {
                            // Get command type
                            FString CommandType;
                            if (JsonObject->TryGetStringField(TEXT("type"), CommandType))
                            {
                                // Execute command
                                FString Response = Bridge->ExecuteCommand(CommandType, JsonObject->GetObjectField(TEXT("params")));

                                // Send response — use UTF-8 byte length, NOT TCHAR character count
                                auto ResponseUtf8 = FTCHARToUTF8(*Response);
                                int32 ResponseBytes = ResponseUtf8.Length();
                                int32 BytesSent = 0;
                                if (!ClientSocket->Send((const uint8*)ResponseUtf8.Get(), ResponseBytes, BytesSent))
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to send response for command '%s'"), *CommandType);
                                }
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Missing 'type' field in command"));
                            }
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to parse JSON"));
                        }
                    }
                    else
                    {
                        int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();
                        // Don't break the connection for WouldBlock error, which is normal for non-blocking sockets
                        bool bShouldBreak = true;

                        if (LastError == SE_EWOULDBLOCK)
                        {
                            bShouldBreak = false;
                            FPlatformProcess::Sleep(0.01f);
                        }
                        else if (LastError == SE_EINTR)
                        {
                            bShouldBreak = false;
                        }
                        else
                        {
                            // Normal disconnect after command — not useful to log
                        }

                        if (bShouldBreak)
                        {
                            break;
                        }
                    }
                }
            }
        }

        // Small sleep to prevent tight loop
        FPlatformProcess::Sleep(0.1f);
    }

    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Server thread stopped"));
    return 0;
}

void FMCPServerRunnable::Stop()
{
    bRunning = false;
}

void FMCPServerRunnable::Exit()
{
}

void FMCPServerRunnable::HandleClientConnection(TSharedPtr<FSocket> InClientSocket)
{
    if (!InClientSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: Invalid client socket"));
        return;
    }

    InClientSocket->SetNonBlocking(false);

    const int32 MaxBufferSize = 4096;
    uint8 Buffer[MaxBufferSize];
    FString MessageBuffer;

    while (bRunning && InClientSocket.IsValid())
    {
        int32 BytesRead = 0;
        bool bReadSuccess = InClientSocket->Recv(Buffer, MaxBufferSize, BytesRead, ESocketReceiveFlags::None);

        if (BytesRead > 0)
        {
            Buffer[BytesRead] = 0;
            FString ReceivedData = UTF8_TO_TCHAR(Buffer);
            MessageBuffer.Append(ReceivedData);

            if (MessageBuffer.Contains(TEXT("\n")))
            {
                TArray<FString> Messages;
                MessageBuffer.ParseIntoArray(Messages, TEXT("\n"), true);

                for (int32 i = 0; i < Messages.Num() - 1; ++i)
                {
                    ProcessMessage(InClientSocket, Messages[i]);
                }

                MessageBuffer = Messages.Last();
            }
        }
        else if (!bReadSuccess)
        {
            break;
        }

        FPlatformProcess::Sleep(0.01f);
    }
}

void FMCPServerRunnable::ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message)
{
    TSharedPtr<FJsonObject> JsonMessage;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

    if (!FJsonSerializer::Deserialize(Reader, JsonMessage) || !JsonMessage.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to parse JSON message"));
        return;
    }

    FString CommandType;
    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());

    if (!JsonMessage->TryGetStringField(TEXT("command"), CommandType))
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Message missing 'command' field"));
        return;
    }

    if (JsonMessage->HasField(TEXT("params")))
    {
        TSharedPtr<FJsonValue> ParamsValue = JsonMessage->TryGetField(TEXT("params"));
        if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
        {
            Params = ParamsValue->AsObject();
        }
    }

    FString Response = Bridge->ExecuteCommand(CommandType, Params);

    // Send response with newline terminator — use UTF-8 byte length, NOT TCHAR character count
    Response += TEXT("\n");
    auto ResponseUtf8 = FTCHARToUTF8(*Response);
    int32 ResponseBytes = ResponseUtf8.Length();
    int32 BytesSent = 0;

    if (!Client->Send((const uint8*)ResponseUtf8.Get(), ResponseBytes, BytesSent))
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to send response for command '%s'"), *CommandType);
    }
}