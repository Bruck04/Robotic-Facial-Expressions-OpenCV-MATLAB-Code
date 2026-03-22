# Robotic-Facial-Expressions-OpenCV-MATLAB-Code
Final year project focused on the development of a real-time facial landmark detection and robotic facial expression system, integrating computer vision with physical actuation. The aim was to create a pipeline capable of capturing human facial movements and reproducing them on a robotic face with minimal latency and high fidelity.

A standard RGB webcam was used to capture live video input, which was processed in MATLAB. Facial landmark detection was implemented using the OpenFace toolkit, enabling the extraction of detailed facial feature coordinates from each frame, including key regions such as the eyes, eyebrows, mouth, and jaw. These landmarks formed the basis for interpreting facial expressions in real time.

The extracted coordinate data was then transmitted from MATLAB to an Arduino microcontroller via serial communication. The Arduino translated this data into control signals for multiple servo motors embedded within the robotic face. Each servo was mapped to specific facial regions, allowing the system to replicate expressions by physically actuating corresponding features, such as raising eyebrows or opening the mouth.

A significant focus of the project was on system synchronisation and performance optimisation. One of the primary challenges encountered was balancing processing speed with detection accuracy. Increasing MATLAB processing speed improved responsiveness but reduced landmark precision, while slower processing enhanced accuracy at the cost of latency. This required careful tuning of frame rates, data filtering, and update intervals to achieve a stable and realistic output.

Additionally, smoothing techniques were implemented to ensure natural-looking motion, reducing jitter caused by frame-to-frame variations in landmark detection. This involved filtering incoming data and optimising servo control signals to produce fluid transitions between expressions rather than abrupt movements.

Overall, the project demonstrates a full end-to-end system, from real-time computer vision and data extraction to embedded control and physical expression synthesis. It highlights key competencies in machine vision, embedded systems, real-time data processing, and system integration, with particular emphasis on bridging software and hardware to create responsive, human-like robotic behaviour.

