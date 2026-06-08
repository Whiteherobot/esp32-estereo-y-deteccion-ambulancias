import sys
import platform

print('=== ENV CHECK ===')
print('python:', sys.version)
print('platform:', platform.platform())

try:
    import cv2
    print('opencv:', cv2.__version__)
except Exception as e:
    print('opencv_error:', e)

try:
    import torch
    print('torch:', torch.__version__)
    print('cuda_available:', torch.cuda.is_available())
    if torch.cuda.is_available():
        print('gpu:', torch.cuda.get_device_name(0))
except Exception as e:
    print('torch_error:', e)
