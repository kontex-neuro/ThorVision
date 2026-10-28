"""Human-detection reference: OpenCV's built-in HOG people detector plus the SDK IoU tracker.

Needs the wheels declared in requirements.in; package preparation stages them
into a per-package environment. The HOG model ships inside OpenCV, so no model
artifact is declared.
"""
import cv2
import numpy

from thorvision_sdk import Detection, FrameResult, Polyline, Rect, Text, TrackPoint
from thorvision_sdk.tracking import IoUTracker


class Processor:
    def setup(self, context, config):
        self.hog = cv2.HOGDescriptor()
        self.hog.setSVMDetector(cv2.HOGDescriptor_getDefaultPeopleDetector())
        self.threshold = config.get('min_confidence', 0.5)
        self.colour = tuple(config.get('colour', (0, 255, 0)))
        self.tracker = IoUTracker(trail_ns=int(config.get('trail_seconds', 2.0) * 1e9))

    def process(self, frame):
        image = numpy.frombuffer(frame.image, numpy.uint8, frame.stride * frame.height)
        image = image.reshape(frame.height, frame.stride)[:, :frame.width * 3].reshape(frame.height, frame.width, 3)
        rects, weights = self.hog.detectMultiScale(cv2.cvtColor(image, cv2.COLOR_RGB2BGR), winStride=(8, 8))
        boxes, scores = [], []
        for (x, y, w, h), weight in zip(rects, numpy.ravel(weights)):
            confidence = float(min(1.0, max(0.0, weight / 2.0)))
            if confidence >= self.threshold:
                boxes.append(frame.transform.box_to_source((x, y, x + w, y + h)))
                scores.append(confidence)
        result = FrameResult()
        for (track_id, box), confidence in zip(self.tracker.update(boxes, frame.capture_time_ns), scores):
            result.detections.append(Detection('person', confidence, box, track_id))
            result.drawings.append(Rect(box, self.colour, 2))
            result.drawings.append(Text(box[0], max(0, box[1] - 16), f'#{track_id}', self.colour, 2, (0, 0, 0)))
            trail = self.tracker.trail(track_id)
            result.track_points.append(TrackPoint(track_id, frame.capture_time_ns, *trail[-1][1:]))
            if len(trail) > 1:
                result.drawings.append(Polyline([point[1:] for point in trail], self.colour, 2))
        result.drawings.append(Text(8, 8, f'people: {len(result.detections)}', (255, 255, 255), 2, (0, 0, 0)))
        return result

    def close(self):
        pass
