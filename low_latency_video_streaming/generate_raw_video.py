#!/usr/bin/env python3
"""
Generate a raw YUV video file for testing the GStreamer streaming server.
Creates a simple test pattern (color bars or gradient) in YUV I420 format.
"""

import argparse
import struct
import math


def generate_color_bar_frame(width, height, frame_num):
    """Generate a single frame of color bars in YUV I420 format."""
    frame_size = width * height * 3 // 2
    y_plane = bytearray(width * height)
    u_plane = bytearray(width * height // 4)
    v_plane = bytearray(width * height // 4)
    
    # Color bar colors (Y, U, V values)
    colors = [
        (235, 128, 128),  # White
        (210, 16, 146),   # Yellow
        (170, 166, 16),   # Cyan
        (145, 34, 234),   # Green
        (107, 240, 202),  # Magenta
        (82, 90, 240),    # Red
        (41, 240, 110),   # Blue
        (16, 128, 128),   # Black
    ]
    
    bar_width = width // len(colors)
    
    for y in range(height):
        for x in range(width):
            bar_idx = min(x // bar_width, len(colors) - 1)
            y_val, u_val, v_val = colors[bar_idx]
            y_plane[y * width + x] = y_val
    
    # Chroma subsampling (4:2:0)
    for y in range(height // 2):
        for x in range(width // 2):
            bar_idx = min((x * 2) // bar_width, len(colors) - 1)
            _, u_val, v_val = colors[bar_idx]
            u_plane[y * (width // 2) + x] = u_val
            v_plane[y * (width // 2) + x] = v_val
    
    # Draw frame number in top-right corner
    draw_frame_number(y_plane, width, height, frame_num)
    
    return bytes(y_plane) + bytes(u_plane) + bytes(v_plane)


def generate_gradient_frame(width, height, frame_num, total_frames):
    """Generate a frame with moving gradient pattern."""
    frame_size = width * height * 3 // 2
    y_plane = bytearray(width * height)
    u_plane = bytearray(width * height // 4)
    v_plane = bytearray(width * height // 4)
    
    offset = (frame_num / total_frames) * math.pi * 2
    
    for y in range(height):
        for x in range(width):
            # Create a moving gradient pattern
            t = (x / width + y / height + offset) % 1.0
            y_val = int(16 + 219 * t)
            y_plane[y * width + x] = y_val
    
    # Chroma subsampling (4:2:0)
    for y in range(height // 2):
        for x in range(width // 2):
            t = ((x * 2) / width + (y * 2) / height + offset) % 1.0
            u_val = int(16 + 224 * math.sin(t * math.pi))
            v_val = int(16 + 224 * math.cos(t * math.pi))
            u_plane[y * (width // 2) + x] = u_val
            v_plane[y * (width // 2) + x] = v_val
    
    # Draw frame number in top-right corner
    draw_frame_number(y_plane, width, height, frame_num)
    
    return bytes(y_plane) + bytes(u_plane) + bytes(v_plane)


def generate_unique_color_frame(width, height, frame_num):
    """Generate a frame with unique color per frame, repeating every 30 frames."""
    frame_size = width * height * 3 // 2
    y_plane = bytearray(width * height)
    u_plane = bytearray(width * height // 4)
    v_plane = bytearray(width * height // 4)
    
    # Cycle through 30 unique colors using HSV color wheel
    cycle_frame = frame_num % 30
    hue = cycle_frame / 30.0  # 0.0 to 1.0
    
    # Convert HSV to RGB
    def hsv_to_rgb(h, s, v):
        i = int(h * 6)
        f = h * 6 - i
        p = v * (1 - s)
        q = v * (1 - f * s)
        t = v * (1 - (1 - f) * s)
        
        if i % 6 == 0:
            r, g, b = v, t, p
        elif i % 6 == 1:
            r, g, b = q, v, p
        elif i % 6 == 2:
            r, g, b = p, v, t
        elif i % 6 == 3:
            r, g, b = p, q, v
        elif i % 6 == 4:
            r, g, b = t, p, v
        else:
            r, g, b = v, p, q
        
        return int(r * 255), int(g * 255), int(b * 255)
    
    # Get RGB color for this frame
    r, g, b = hsv_to_rgb(hue, 1.0, 1.0)
    
    # Convert RGB to YUV (BT.601 standard)
    y_val = int(0.299 * r + 0.587 * g + 0.114 * b)
    u_val = int(-0.169 * r - 0.331 * g + 0.500 * b + 128)
    v_val = int(0.500 * r - 0.419 * g - 0.081 * b + 128)
    
    # Clamp values to valid range
    y_val = max(16, min(235, y_val))
    u_val = max(16, min(240, u_val))
    v_val = max(16, min(240, v_val))
    
    # Fill Y plane with the Y value
    for i in range(width * height):
        y_plane[i] = y_val
    
    # Fill U and V planes with the U and V values (chroma subsampling 4:2:0)
    for i in range(width * height // 4):
        u_plane[i] = u_val
        v_plane[i] = v_val
    
    # Draw frame number in top-right corner
    draw_frame_number(y_plane, width, height, frame_num)
    
    return bytes(y_plane) + bytes(u_plane) + bytes(v_plane)


def draw_frame_number(y_plane, width, height, frame_num):
    """Draw frame number in top-right corner using simple 5x7 bitmap font."""
    # Simple 5x7 bitmap font for digits 0-9
    digit_patterns = {
        '0': [
            "  ###  ",
            " #   # ",
            " #   # ",
            " #   # ",
            " #   # ",
            " #   # ",
            "  ###  "
        ],
        '1': [
            "  ##   ",
            "   #   ",
            "   #   ",
            "   #   ",
            "   #   ",
            "   #   ",
            "  ###  "
        ],
        '2': [
            "  ###  ",
            " #   # ",
            "     # ",
            "    ## ",
            "   #   ",
            "  #    ",
            " ##### "
        ],
        '3': [
            "  ###  ",
            " #   # ",
            "     # ",
            "   ##  ",
            "     # ",
            " #   # ",
            "  ###  "
        ],
        '4': [
            "   ##  ",
            "  # #  ",
            " #  #  ",
            " ##### ",
            "    #  ",
            "    #  ",
            "    #  "
        ],
        '5': [
            " ##### ",
            " #     ",
            " ####  ",
            "     # ",
            "     # ",
            " #   # ",
            "  ###  "
        ],
        '6': [
            "  ###  ",
            " #     ",
            " ####  ",
            " #   # ",
            " #   # ",
            " #   # ",
            "  ###  "
        ],
        '7': [
            " ##### ",
            "     # ",
            "    #  ",
            "   #   ",
            "   #   ",
            "   #   ",
            "   #   "
        ],
        '8': [
            "  ###  ",
            " #   # ",
            " #   # ",
            "  ###  ",
            " #   # ",
            " #   # ",
            "  ###  "
        ],
        '9': [
            "  ###  ",
            " #   # ",
            " #   # ",
            "  #### ",
            "     # ",
            " #   # ",
            "  ###  "
        ]
    }
    
    # Convert frame number to string
    frame_str = str(frame_num)
    
    # Calculate text dimensions
    digit_width = 7  # Must match pattern string length
    digit_height = 7
    spacing = 0  # No spacing to eliminate calculation issues
    total_width = len(frame_str) * digit_width + 1  # Add 1 for safety margin
    total_height = digit_height
    
    # Scale factor for larger text
    scale = 10
    
    # Position in center of screen
    start_x = (width - total_width * scale) // 2
    start_y = (height - total_height * scale) // 2
    
    # Draw white background as a very large fixed rectangle centered on screen
    # This ensures it always covers all digits regardless of calculation
    bg_width = 1200 * scale  # Very wide to ensure full coverage
    bg_height = 300 * scale  # Very tall
    bg_start_x = (width - bg_width) // 2
    bg_start_y = (height - bg_height) // 2
    bg_end_x = bg_start_x + bg_width
    bg_end_y = bg_start_y + bg_height
    
    # Draw white background rectangle
    for y in range(max(0, bg_start_y), min(height, bg_end_y)):
        for x in range(max(0, bg_start_x), min(width, bg_end_x)):
            y_plane[y * width + x] = 235  # White (high Y value)
    
    # Draw each digit
    for char_idx, char in enumerate(frame_str):
        if char not in digit_patterns:
            continue
        
        pattern = digit_patterns[char]
        # Calculate position: each digit takes digit_width * scale pixels
        # But the last pixel is at col*scale + (scale-1), so we need to account for this
        digit_start_x = start_x + char_idx * digit_width * scale
        
        for row in range(digit_height):
            for col in range(digit_width):
                if pattern[row][col] == '#':
                    # Draw scaled pixel
                    for sy in range(scale):
                        for sx in range(scale):
                            x = digit_start_x + col * scale + sx
                            y = start_y + row * scale + sy
                            if 0 <= x < width and 0 <= y < height:
                                # Set pixel to black (low Y value) on white background
                                y_plane[y * width + x] = 16


def generate_raw_video(output_file, width, height, framerate, duration, pattern):
    """Generate a raw YUV video file."""
    total_frames = framerate * duration
    frame_size = width * height * 3 // 2
    
    print(f"Generating {pattern} pattern video:")
    print(f"  Resolution: {width}x{height}")
    print(f"  Framerate: {framerate} FPS")
    print(f"  Duration: {duration} seconds")
    print(f"  Total frames: {total_frames}")
    print(f"  Output file: {output_file}")
    
    with open(output_file, 'wb') as f:
        for frame_num in range(total_frames):
            if frame_num % 10 == 0:
                print(f"  Progress: {frame_num}/{total_frames} frames ({100*frame_num//total_frames}%)")
            
            if pattern == 'colorbars':
                frame_data = generate_color_bar_frame(width, height, frame_num)
            elif pattern == 'gradient':
                frame_data = generate_gradient_frame(width, height, frame_num, total_frames)
            elif pattern == 'unique':
                frame_data = generate_unique_color_frame(width, height, frame_num)
            else:
                raise ValueError(f"Unknown pattern: {pattern}")
            
            f.write(frame_data)
    
    print(f"Complete! File size: {frame_size * total_frames / (1024*1024):.2f} MB")


def main():
    parser = argparse.ArgumentParser(description='Generate raw YUV video file for testing')
    parser.add_argument('--output', default='test.yuv',
                        help='Output file path (default: test.yuv)')
    parser.add_argument('--width', type=int, default=1920,
                        help='Video width in pixels (default: 1920)')
    parser.add_argument('--height', type=int, default=1080,
                        help='Video height in pixels (default: 1080)')
    parser.add_argument('--framerate', type=int, default=30,
                        help='Video framerate (default: 30)')
    parser.add_argument('--duration', type=int, default=10,
                        help='Video duration in seconds (default: 10)')
    parser.add_argument('--pattern', choices=['colorbars', 'gradient', 'unique'], default='colorbars',
                        help='Test pattern type (default: colorbars)')
    
    args = parser.parse_args()
    
    generate_raw_video(
        output_file=args.output,
        width=args.width,
        height=args.height,
        framerate=args.framerate,
        duration=args.duration,
        pattern=args.pattern
    )


if __name__ == '__main__':
    main()
