# USB Cameras

## Camera Compatibility

### UVC Camera

The Universal Video Class (UVC) camera is a standard USB camera that adheres to the UVC protocol, offering plug-and-play support with modern operating systems. However, while UVC cameras are widely supported, their performance can vary substantially based on factors such as sensor design, cable quality, and especially the available USB bandwidth.

We have successfully tested the following camera configuration on our system:

| Camera     | Resolution | FPS | Max. Cameras | USB   |
|------------|------------|-----|--------------|-------|
| UVC Camera | 1280x720   | 30  | 4            | 2.0   |

**Supplementary Note:**  
Some camera models may not achieve these performance metrics due to limitations in USB bandwidth acquisition. In multi-camera setups or when a single camera requires higher data throughput, the available USB bandwidth can become a bottleneck, potentially resulting in lower resolution or frame rates than expected. It is recommended to verify camera and system compatibility—especially regarding USB bandwidth—when planning your deployment.












