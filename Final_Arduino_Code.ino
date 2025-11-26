#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Create an instance of the PCA9685 driver object
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Define the baud rate for serial communication.
#define BAUD_RATE 115200

// Define the number of landmarks you expect to receive from MATLAB.
const int NUM_EXPECTED_LANDMARKS = 15;

// Global arrays to store the X and Y coordinates of the selected landmarks.
float received_landmark_x[NUM_EXPECTED_LANDMARKS];
float received_landmark_y[NUM_EXPECTED_LANDMARKS];
int current_landmark_storage_idx = 0;

// --- Servo Channels on PCA9685 (0-15) ---
// !!! YOU MUST DEFINE THESE CHANNEL NUMBERS CAREFULLY - NO OVERLAPS !!!
const int JAW_SERVO_LEFT_CHANNEL = 0;  // Example
const int JAW_SERVO_RIGHT_CHANNEL = 1; // Example
const int R_EYELID_SERVO_CHANNEL = 2;  // Example
const int L_EYELID_SERVO_CHANNEL = 3;  // Example

// --- Servo Pulse Lengths & Frequency for PCA9685 ---
#define SERVO_FREQ 60 // Hz, common for SG90s
#define SERVOMIN_US 500
#define SERVOMAX_US 2500

// --- !!! CRITICAL CALIBRATION CONSTANTS - YOU MUST DETERMINE THESE EXPERIMENTALLY !!! ---

// == JAW CALIBRATION CONSTANTS ==
// For Jaw Servo Angles (degrees) - Calibrate for EACH jaw servo
int LEFT_JAW_SERVO_CLOSED_ANGLE_DEG = 0;
int LEFT_JAW_SERVO_OPEN_ANGLE_DEG = 70;
int RIGHT_JAW_SERVO_CLOSED_ANGLE_DEG = 70; 
int RIGHT_JAW_SERVO_OPEN_ANGLE_DEG = 0;   

// For Normalised Mouth Openness Metric (Normalised by IOD) - Hysteresis Thresholds
// --- !!! HOW TO CALIBRATE JAW HYSTERESIS FOR "CLEARLY OBVIOUS" OPEN/CLOSE (INVERTED SIGNAL) !!! ---
// You observed: `Raw NormMouthOpen` is SMALLER when your mouth is OPEN (e.g., ~0.05)
//             and LARGER when your mouth is CLOSED (e.g., ~0.11).
// The logic `if (norm_mouth_openness < MOUTH_NORM_FOR_OPENING)` means "OPEN".
//
// 1. PHYSICALLY OPEN YOUR MOUTH (to the point you want animatronic to register as "clearly open"):
//    Observe the HIGHEST `Raw NormMouthOpen` value it jitters to. Let this be `your_actual_max_open_jaw_reading`.
//    (e.g., if "clearly open" is ~0.05, it might jitter up to 0.065)
// 2. PHYSICALLY CLOSE YOUR MOUTH (to the point you want animatronic to register as "clearly closed"):
//    Observe the LOWEST `Raw NormMouthOpen` value it jitters to.
//    Let this be `your_actual_min_closed_jaw_reading`.
//    (e.g., if "clearly closed" is ~0.11, it might jitter down to 0.100)
// 3. SET THE HYSTERESIS THRESHOLDS to create a robust dead-band:
//    - `MOUTH_NORM_FOR_OPENING`: Set this slightly HIGHER than `your_actual_max_open_jaw_reading`.
//                                The jaw opens if `Raw NormMouthOpen` drops BELOW this.
//                                (e.g., if `your_actual_max_open_jaw_reading` is 0.065, try MOUTH_NORM_FOR_OPENING = 0.075)
//    - `MOUTH_NORM_FOR_CLOSING`: Set this slightly LOWER than `your_actual_min_closed_jaw_reading`.
//                                The jaw closes if `Raw NormMouthOpen` goes ABOVE this.
//                                (e.g., if `your_actual_min_closed_jaw_reading` is 0.100, try MOUTH_NORM_FOR_CLOSING = 0.095)
//    Ensure `MOUTH_NORM_FOR_CLOSING` is still > `MOUTH_NORM_FOR_OPENING` for the dead-band.
//    Adjust these to make the dead-band wider if it still jitters/flickers.
//
float MOUTH_NORM_FOR_OPENING = 0.075; // Example for INVERTED JAW: Calibrate this!
float MOUTH_NORM_FOR_CLOSING = 0.095; // Example for INVERTED JAW: Calibrate this!

// == EYELID CALIBRATION CONSTANTS ==
int R_EYELID_SERVO_CLOSED_ANGLE_DEG = 20;
int R_EYELID_SERVO_OPEN_ANGLE_DEG = 80;
int L_EYELID_SERVO_CLOSED_ANGLE_DEG = 80; 
int L_EYELID_SERVO_OPEN_ANGLE_DEG = 20;   

// For Normalised Eye Openness Metric (based on RIGHT eye landmarks) - Hysteresis Thresholds
// This assumes your Raw RNormEyeOpen is SMALLER when your eye is OPEN, and LARGER when CLOSED (INVERTED signal for eye).
float EYE_OPEN_NORM_FOR_OPENING = 0.081; 
float EYE_OPEN_NORM_FOR_CLOSING = 0.084; 

// State variables
bool are_eyelids_commanded_open = true; 
bool is_jaw_commanded_open = false;     

// --- Define NAMED INDICES for accessing the received_landmark arrays. ---
const int IDX_L_EYE_CORNER_IOD = 0;
const int IDX_R_EYE_CORNER_IOD = 1;
const int IDX_R_EYE_UP_1 = 2;
const int IDX_R_EYE_UP_2 = 3;
const int IDX_R_EYE_LOW_1 = 4;
const int IDX_R_EYE_LOW_2 = 5;
const int IDX_MOUTH_UPPER_LIP = 10;
const int IDX_MOUTH_LOWER_LIP = 11;

void setup() {
    Serial.begin(BAUD_RATE);
    Serial.setTimeout(50); 
    Serial.println("Arduino Combined Jaw & Eyelid Control (PCA9685 - Calibrate Jaw Hysteresis) Initialised.");
    Serial.println("Baud rate: " + String(BAUD_RATE));

    pwm.begin();
    pwm.setPWMFreq(SERVO_FREQ);
    delay(10);

    setServoAngle(JAW_SERVO_LEFT_CHANNEL, LEFT_JAW_SERVO_CLOSED_ANGLE_DEG);
    setServoAngle(JAW_SERVO_RIGHT_CHANNEL, RIGHT_JAW_SERVO_CLOSED_ANGLE_DEG);
    is_jaw_commanded_open = false;
    
    setServoAngle(R_EYELID_SERVO_CHANNEL, R_EYELID_SERVO_OPEN_ANGLE_DEG);
    setServoAngle(L_EYELID_SERVO_CHANNEL, L_EYELID_SERVO_OPEN_ANGLE_DEG);
    are_eyelids_commanded_open = true; 

    Serial.println("PCA9685 initialised. Servos set to initial positions.");
    Serial.println("--- Calibrate all NORM and ANGLE constants carefully! ---");
    Serial.println("Jaw OpenTh (val<this): " + String(MOUTH_NORM_FOR_OPENING, 4) + ", Jaw CloseTh (val>this): " + String(MOUTH_NORM_FOR_CLOSING, 4));
    Serial.println("Eye OpenTh (val<this): " + String(EYE_OPEN_NORM_FOR_OPENING, 4) + ", Eye CloseTh (val>this): " + String(EYE_OPEN_NORM_FOR_CLOSING, 4));
}

void loop() {
    if (Serial.available() > 0) {
        String full_data_string = Serial.readStringUntil('\n');
        full_data_string.trim();

        if (full_data_string.length() > 0) {
            int segment_start_index = 0;
            current_landmark_storage_idx = 0; 

            for (int i = 0; i < NUM_EXPECTED_LANDMARKS; i++) {
                int semicolon_pos = full_data_string.indexOf(';', segment_start_index);
                if (semicolon_pos == -1) { 
                    if (segment_start_index < full_data_string.length()){ 
                        String last_segment = full_data_string.substring(segment_start_index);
                        last_segment.trim();
                        if(last_segment.length() > 0) parse_and_store_segment(last_segment);
                    }
                    break; 
                }
                String segment = full_data_string.substring(segment_start_index, semicolon_pos);
                segment.trim();
                if (segment.length() > 0) {
                    parse_and_store_segment(segment);
                }
                segment_start_index = semicolon_pos + 1;
            }
            
            if (current_landmark_storage_idx == NUM_EXPECTED_LANDMARKS) {
                process_all_motors(); 
            }
        }
    }
}

bool parse_and_store_segment(String segment_str) {
    int colon_pos = segment_str.indexOf(':');
    int comma_pos = segment_str.indexOf(',');

    if (colon_pos > 0 && comma_pos > colon_pos + 1 && comma_pos < segment_str.length() - 1) {
        String x_str = segment_str.substring(colon_pos + 1, comma_pos);
        String y_str = segment_str.substring(comma_pos + 1);
        float x_val = x_str.toFloat();
        float y_val = y_str.toFloat();

        if (current_landmark_storage_idx < NUM_EXPECTED_LANDMARKS) {
            received_landmark_x[current_landmark_storage_idx] = x_val;
            received_landmark_y[current_landmark_storage_idx] = y_val;
            current_landmark_storage_idx++;
            return true;
        }
        return false; 
    } else {
        return false;
    }
}

int angleToPulseTicks(int ang_deg) {
    long pulse_us = map(ang_deg, 0, 180, SERVOMIN_US, SERVOMAX_US);
    float us_per_tick = 1000000.0f / SERVO_FREQ / 4096.0f;
    if (us_per_tick < 0.001f) { 
         us_per_tick = (SERVO_FREQ == 60) ? (1000000.0f / 60.0f / 4096.0f) : (1000000.0f / 50.0f / 4096.0f);
    }
    return int(float(pulse_us) / us_per_tick);
}

void setServoAngle(uint8_t channel, int angle_deg) {
    if (channel >= 0 && channel < 16) { 
        int pulse_ticks = angleToPulseTicks(angle_deg);
        pulse_ticks = constrain(pulse_ticks, 0, 4095); 
        pwm.setPWM(channel, 0, pulse_ticks);
    }
}

void process_all_motors() { 
    float iod = abs(received_landmark_x[IDX_R_EYE_CORNER_IOD] - received_landmark_x[IDX_L_EYE_CORNER_IOD]);
    if (iod < 10.0) { 
        return;
    }

    // --- JAW MOTOR LOGIC (Dual Jaw Servos, Hysteresis Threshold-based for INVERTED signal) ---
    float upper_lip_y = received_landmark_y[IDX_MOUTH_UPPER_LIP];
    float lower_lip_y = received_landmark_y[IDX_MOUTH_LOWER_LIP];
    float raw_mouth_openness = abs(lower_lip_y - upper_lip_y); 
    float norm_mouth_openness = raw_mouth_openness / iod;       
    
    Serial.print("RawNormMouthOpen: "); Serial.print(norm_mouth_openness, 4); 

    int jaw_left_target_angle_deg;
    int jaw_right_target_angle_deg;

    // Hysteresis logic for JAW, assuming INVERTED signal (smaller norm_mouth_openness = more open)
    if (is_jaw_commanded_open) { // If jaw is currently commanded open
        // To CLOSE it, your actual mouth needs to close more.
        // This means norm_mouth_openness needs to go ABOVE the MOUTH_NORM_FOR_CLOSING threshold.
        if (norm_mouth_openness > MOUTH_NORM_FOR_CLOSING) { 
            jaw_left_target_angle_deg = LEFT_JAW_SERVO_CLOSED_ANGLE_DEG;
            jaw_right_target_angle_deg = RIGHT_JAW_SERVO_CLOSED_ANGLE_DEG; 
            is_jaw_commanded_open = false;
            Serial.print(" ->MouthClosing");
        } else { // Stays open
            jaw_left_target_angle_deg = LEFT_JAW_SERVO_OPEN_ANGLE_DEG;
            jaw_right_target_angle_deg = RIGHT_JAW_SERVO_OPEN_ANGLE_DEG; 
            Serial.print(" ->MouthStayingOpen");
        }
    } else { // If jaw is currently commanded closed
        // To OPEN it, your actual mouth needs to open more.
        // This means norm_mouth_openness needs to go BELOW the MOUTH_NORM_FOR_OPENING threshold.
        if (norm_mouth_openness < MOUTH_NORM_FOR_OPENING) { 
            jaw_left_target_angle_deg = LEFT_JAW_SERVO_OPEN_ANGLE_DEG;
            jaw_right_target_angle_deg = RIGHT_JAW_SERVO_OPEN_ANGLE_DEG; 
            is_jaw_commanded_open = true;
            Serial.print(" ->MouthOpening");
        } else { // Stays closed
            jaw_left_target_angle_deg = LEFT_JAW_SERVO_CLOSED_ANGLE_DEG;
            jaw_right_target_angle_deg = RIGHT_JAW_SERVO_CLOSED_ANGLE_DEG; 
            Serial.print(" ->MouthStayingClosed");
        }
    }
    setServoAngle(JAW_SERVO_LEFT_CHANNEL, jaw_left_target_angle_deg);
    setServoAngle(JAW_SERVO_RIGHT_CHANNEL, jaw_right_target_angle_deg);
    Serial.print(",JawL: "); Serial.print(jaw_left_target_angle_deg);
    Serial.print(",JawR: "); Serial.print(jaw_right_target_angle_deg);


    // --- EYELID MOTOR LOGIC (Synced Dual Eyelids, Hysteresis based on Right Eye - INVERTED SIGNAL LOGIC) ---
    float r_eye_upper_avg_y = (received_landmark_y[IDX_R_EYE_UP_1] + received_landmark_y[IDX_R_EYE_UP_2]) / 2.0f;
    float r_eye_lower_avg_y = (received_landmark_y[IDX_R_EYE_LOW_1] + received_landmark_y[IDX_R_EYE_LOW_2]) / 2.0f;
    float raw_r_eye_openness = abs(r_eye_lower_avg_y - r_eye_upper_avg_y); 
    float norm_r_eye_openness = raw_r_eye_openness / iod;       
    
    Serial.print(" | RawRNormEyeOpen: "); Serial.print(norm_r_eye_openness, 4); 

    int r_eyelid_target_angle_deg;
    int l_eyelid_target_angle_deg;

    // This logic is for an INVERTED signal: SMALLER norm_r_eye_openness means your right eye is MORE open.
    if (are_eyelids_commanded_open) { 
      if (norm_r_eye_openness > EYE_OPEN_NORM_FOR_CLOSING) { 
        r_eyelid_target_angle_deg = R_EYELID_SERVO_CLOSED_ANGLE_DEG;
        l_eyelid_target_angle_deg = L_EYELID_SERVO_CLOSED_ANGLE_DEG; 
        are_eyelids_commanded_open = false; 
        Serial.print(" ->EyelidsClosing (Inv)");
      } else {
        r_eyelid_target_angle_deg = R_EYELID_SERVO_OPEN_ANGLE_DEG;
        l_eyelid_target_angle_deg = L_EYELID_SERVO_OPEN_ANGLE_DEG; 
        Serial.print(" ->EyelidsStayingOpen (Inv)");
      }
    } else { 
      if (norm_r_eye_openness < EYE_OPEN_NORM_FOR_OPENING) { 
        r_eyelid_target_angle_deg = R_EYELID_SERVO_OPEN_ANGLE_DEG;
        l_eyelid_target_angle_deg = L_EYELID_SERVO_OPEN_ANGLE_DEG; 
        are_eyelids_commanded_open = true; 
        Serial.print(" ->EyelidsOpening (Inv)");
      } else {
        r_eyelid_target_angle_deg = R_EYELID_SERVO_CLOSED_ANGLE_DEG;
        l_eyelid_target_angle_deg = L_EYELID_SERVO_CLOSED_ANGLE_DEG; 
        Serial.print(" ->EyelidsStayingClosed (Inv)");
      }
    }
    setServoAngle(R_EYELID_SERVO_CHANNEL, r_eyelid_target_angle_deg);
    setServoAngle(L_EYELID_SERVO_CHANNEL, l_eyelid_target_angle_deg);

    Serial.print(",REyeAngle: "); Serial.print(r_eyelid_target_angle_deg);
    Serial.print(",LEyeAngle: "); Serial.println(l_eyelid_target_angle_deg);
}
