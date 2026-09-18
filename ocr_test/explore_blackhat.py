import glob
import os
import subprocess

import cv2
import numpy as np


IMAGES = {
    "1": r"C:\Users\Matt\Pictures\wardogs\1.PNG",
    "2": r"C:\Users\Matt\Pictures\wardogs\2.PNG",
    "3": r"C:\Users\Matt\Pictures\wardogs\3.PNG",
}
OUTPUT = r"E:\Projects\WarDogsArtillery\ocr_test\blackhat"
TESSERACT = r"C:\Program Files\Tesseract-OCR\tesseract.exe"
TESSDATA = r"E:\Projects\WarDogsArtillery\build\tessdata"

os.makedirs(OUTPUT, exist_ok=True)
for name, path in IMAGES.items():
    image = cv2.imread(path)
    crop = image[330:750, 615:725]
    gray = cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY)
    for kernel_size in (9, 15, 21, 31, 41):
        kernel = cv2.getStructuringElement(
            cv2.MORPH_RECT, (kernel_size, kernel_size))
        blackhat = cv2.morphologyEx(gray, cv2.MORPH_BLACKHAT, kernel)
        for threshold in (5, 10, 15, 20, 25, 30, 40):
            _, binary = cv2.threshold(
                blackhat, threshold, 255, cv2.THRESH_BINARY)
            enlarged = cv2.resize(
                binary, None, fx=4, fy=4, interpolation=cv2.INTER_NEAREST)
            output_path = os.path.join(
                OUTPUT, f"{name}_k{kernel_size}_t{threshold}.png")
            cv2.imwrite(output_path, enlarged)

for name in IMAGES:
    print(f"--- {name} ---")
    paths = sorted(glob.glob(os.path.join(OUTPUT, f"{name}_*.png")))
    for path in paths:
        result = subprocess.run(
            [
                TESSERACT,
                path,
                "stdout",
                "--tessdata-dir",
                TESSDATA,
                "--oem",
                "1",
                "--psm",
                "11",
                "-c",
                "tessedit_char_whitelist=0123456789Mm",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        text = " ".join(result.stdout.split())
        if text:
            print(os.path.basename(path), repr(text))
