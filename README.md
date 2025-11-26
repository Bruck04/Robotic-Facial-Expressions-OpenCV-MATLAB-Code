# Robotic-Facial-Expressions-OpenCV-MATLAB-Code
Final year project for real-time facial landmark detection and robotic face expression. MATLAB extracts landmarks from a webcam feed and sends data to an Arduino, which drives servos to mirror human facial movements. Focus on synchronising vision processing with smooth robotic actuation.

This project implements real-time facial landmark detection and transfers the data to a robotic face to synthesise human expressions. A standard RGB webcam captures live video, and MATLAB processes each frame to extract facial landmark coordinates using the OpenFace toolkit https://github.com/TadasBaltrusaitis/OpenFace . The extracted data is then sent to an Arduino, which drives servos to animate the robotic face based on the detected expressions.

The system demonstrates the complete pipeline from computer vision to physical actuation. Key challenges addressed include synchronising MATLAB processing speed with robotic actuation, maintaining landmark detection accuracy, and achieving smooth servo motion.
