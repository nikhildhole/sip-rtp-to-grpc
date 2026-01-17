import grpc
from concurrent import futures
import time

from proto import voicebot_pb2
from proto import voicebot_pb2_grpc


class VoiceBotServicer(voicebot_pb2_grpc.VoiceBotServicer):
    def StreamCall(self, request_iterator, context):
        """
        Echo back:
        - Any incoming AudioChunk -> send it back as CallAction.audio
        - If ControlEvent.HANGUP -> send CallAction.control(HANGUP) and end
        """

        call_id = None
        got_config = False

        for event in request_iterator:
            call_id = event.call_id or call_id

            which = event.WhichOneof("payload")

            if which == "config":
                cfg = event.config
                got_config = True
                print(f"[{call_id}] Config received: sample_rate={cfg.sample_rate}, codec={cfg.codec}")
                # No response needed for config, but you could send an acknowledgement if you wanted
                continue

            if which == "audio":
                audio = event.audio
                if not got_config:
                    print(f"[{call_id}] Warning: audio received before config")

                print(f"[{call_id}] Audio seq={audio.seq}, ts={audio.timestamp}, bytes={len(audio.data)}")

                # Echo it back directly
                yield voicebot_pb2.CallAction(
                    audio=voicebot_pb2.AudioChunk(
                        seq=audio.seq,
                        timestamp=audio.timestamp,
                        data=audio.data
                    )
                )

            if which == "control":
                ctrl = event.control
                print(f"[{call_id}] Control event: {ctrl.type}")

                # Echo hangup action back and stop streaming
                if ctrl.type == voicebot_pb2.ControlEvent.Type.HANGUP:
                    yield voicebot_pb2.CallAction(
                        control=voicebot_pb2.BotControl(
                            type=voicebot_pb2.BotControl.Type.HANGUP
                        )
                    )
                    break

        print(f"[{call_id}] StreamCall ended")


def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    voicebot_pb2_grpc.add_VoiceBotServicer_to_server(VoiceBotServicer(), server)

    server.add_insecure_port("[::]:50051")
    server.start()
    print("Echo VoiceBot gRPC server listening on :50051")

    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        print("Shutting down...")
        server.stop(0)


if __name__ == "__main__":
    serve()