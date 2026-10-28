"""Drawing-contract example: a moving green box and the frame number."""
from thorvision_sdk import FrameResult, Rect, Text


class Processor:
    def setup(self, context, config):
        self.size = config.get('box_size', 80)
        self.speed = config.get('pixels_per_second', 120)

    def process(self, frame):
        span = max(1, frame.source_width - self.size)
        travelled = int(frame.capture_time_ns * self.speed // 1_000_000_000) % (2 * span)
        x = travelled if travelled <= span else 2 * span - travelled
        y = (frame.source_height - self.size) / 2
        return FrameResult(drawings=[
            Rect((x, y, x + self.size, y + self.size), (0, 255, 0), 3),
            Text(8, 8, f'frame {frame.ref.sequence}', (255, 255, 255), 2, (0, 0, 0)),
        ])

    def close(self):
        pass
