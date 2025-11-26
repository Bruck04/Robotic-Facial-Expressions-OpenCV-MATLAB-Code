clear; clc; close all;
%% Add required paths
addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\model_training\pdm_generation\PDM_helpers');
%addpath(genpath('../fitting/'));
addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\model_training\AU_training\experiments\UNBC\models');
addpath(genpath('../face_detection')); % Make sure this relative path is correct from where you run the script
addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\model_training\CCNF');
addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\matlab_version\models');
addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\matlab_version\face_detection');
addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\matlab_version\fitting');
addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\matlab_version\fitting\normxcorr2_mex_ALL\');
%addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\matlab_version\models\');

% --- Define Selected Landmark Indices (0-indexed standard, converted to 1-indexed for MATLAB) ---
% For: IOD (36,45), R_Eye_Blink (37,38,40,41), L_Eye_Blink (43,44,46,47), MouthOpen (62,66), MouthCorners (48,54), NoseRef(33)
selected_indices_0based = [36, 45, 37, 38, 40, 41, 43, 44, 46, 47, 62, 66, 48, 54, 33];
selected_indices_matlab = selected_indices_0based + 1; % Convert to 1-based indexing for MATLAB
num_selected_landmarks = length(selected_indices_matlab);
% --- End of definition ---

%% Load the facial landmark model (CLM is fast but less accurate)
[patches, pdm, clmParams] = Load_CLM_general();

% Multi-view setup (head poses)
views = [0,0,0; 0,-30,0; 0,30,0; 0,0,30; 0,0,-30;];
views = views * pi/180; % Convert degrees to radians
% Setup for face detection
%setup_mconvnet;
addpath('C:\Users\Petro\Downloads\OpenFace-master\OpenFace-master\matlab_version\demo');
setup_mconvnet;

%% Setup Serial Communication with Arduino
arduino_port = "COM4"; % Change this to your Arduino's port
baud_rate = 115200;      % Match with Arduino's baud rate
device = serialport(arduino_port, baud_rate);
flush(device);         % Clear any previous data from the serial buffer
pause(2);              % Allow time for connection setup

%% Setup Webcam and Plot
verbose = true;
cam = webcam("1080p FHD Camera", "Resolution", "320x240"); % Ensure this webcam name is correct for your system
h = imagesc(snapshot(cam)); % Handle for the image object
axis equal; % Set axis properties once to maintain aspect ratio
axis off;   % Turn off axis numbering and ticks
colormap gray; % Use greyscale colormap if displaying greyscale, or remove/change if displaying colour
evt = 0;    % Event flag for scatter plot initialisation
% xdata_history = []; % Retain if you want to store historical data of all landmarks
% ydata_history = []; % (Using a more descriptive name)

%% Main Loop for Face Tracking
for k = 1:10000 % Increased loop count for longer testing if needed
    image_orig = snapshot(cam); % Capture frame
    
    % Face detection
    [bboxs] = detect_faces(image_orig, 'cascade');
    
    % Convert to greyscale if needed (face detection and fitting often use greyscale)
    if size(image_orig,3) == 3
        image_gray = rgb2gray(image_orig);
    else
        image_gray = image_orig;
    end
    
    % Update image display (display the original colour image or greyscale)
    if verbose
        set(h, "CData", image_orig); % Display original colour image
        hold on; % Keep hold on for subsequent plots within this iteration
    end
    
    if isempty(bboxs)
        if verbose && evt == 1 && exist('h2','var') && isvalid(h2)
            set(h2, "XData", [], "YData", []); % Clear landmarks if no face detected
        end
        if verbose
            hold off; % Release hold if no faces found in this frame
        end
        drawnow; % Update figure window to reflect changes
        continue; % Skip to the next frame
    end

    % Process each detected face (typically we focus on the first/largest one)
    for i = 1:size(bboxs,1)
        bbox = bboxs(i,:);
        
        % Fit face landmarks
        [shape_all_68,~,~,~,~,view_used] = ...
            Fitting_from_bb_multi_hyp(image_gray, [], bbox, pdm, patches, clmParams, views);
        
        if(isempty(shape_all_68)) % Check if landmark fitting failed for this bounding box
            if verbose && i == size(bboxs,1) % If it's the last bounding box and verbose is on
                 hold off;
            end
            continue; % Skip to the next detected face
        end

        shape_all_68 = shape_all_68 + 1; % Adjust for MATLAB 1-based indexing

        % --- START OF MODIFIED SECTION FOR DATA SENDING (Key-Value Style) ---
        % Extract only the selected 15 landmarks for sending
        
        data_str = ""; % Initialise empty string
        for j = 1:num_selected_landmarks
            matlab_idx = selected_indices_matlab(j); % The 1-based index for 'shape_all_68'
            standard_idx = selected_indices_0based(j); % The 0-based standard landmark number (e.g., 36)

            % Get X and Y coordinates for the current selected landmark
            x_coord = shape_all_68(matlab_idx, 1); % X coordinate
            y_coord = shape_all_68(matlab_idx, 2); % Y coordinate

            % Format: "StandardIndex:X,Y;" e.g., "36:137.34,178.56;"
            data_str = data_str + sprintf('%d:%0.2f,%0.2f;', standard_idx, x_coord, y_coord);
        end
        % --- END OF MODIFIED SECTION FOR DATA SENDING ---

        % Send data to Arduino
        writeline(device, data_str);
        disp("Sent (Key-Value, " + num2str(num_selected_landmarks) + " landmarks): " + data_str);
        
        % --- VISUALISATION PART (Plot all visible landmarks from the 68 set) ---
        plot_points_x = [];
        plot_points_y = [];
        if ~isempty(patches(1).visibilities) && view_used > 0 && view_used <= size(patches(1).visibilities,1)
            v_points = logical(patches(1).visibilities(view_used,:));
            if length(v_points) == size(shape_all_68,1) % Ensure v_points matches shape_all_68
                if sum(v_points) > 0 % If there are any visible points
                     plot_points_x = shape_all_68(v_points,1);
                     plot_points_y = shape_all_68(v_points,2);
                end
            end
        end

        % Update scatter plot
        if evt == 0 && ~isempty(plot_points_x)
            % Initialise scatter plot for landmarks
            h2 = scatter(plot_points_x, plot_points_y, 50, "red", "filled", 'MarkerEdgeColor', 'k', 'LineWidth', 0.5); % Size 50
            evt = 1;
        elseif evt == 1 && exist('h2','var') && isvalid(h2) % If plot exists and is valid
            if ~isempty(plot_points_x)
                set(h2, "XData", plot_points_x, "YData", plot_points_y);
            else
                set(h2, "XData", [], "YData", []); % Clear points if none are visible
            end
        elseif evt == 1 && (~exist('h2','var') || ~isvalid(h2)) && ~isempty(plot_points_x)
             % Reinitialise h2 if it was deleted or became invalid
             h2 = scatter(plot_points_x, plot_points_y, 50, "red", "filled", 'MarkerEdgeColor', 'k', 'LineWidth', 0.5);
             evt = 1; % Ensure evt is still 1
        end
        
        % xdata_history = [xdata_history; shape_all_68(:,1)']; % Uncomment if you want to store historical data
        % ydata_history = [ydata_history; shape_all_68(:,2)']; % Uncomment if you want to store historical data
        
        % Only process the first detected face for simplicity in this loop iteration.
        % If you want to process all detected faces, remove this 'break'.
        break; 
    end % End of loop for processing each detected face (bboxs)
    
    if verbose
        hold off; % Release hold after all plotting for this frame is done
    end
    drawnow; % Update figure window, process callbacks (important for responsiveness)
    pause(0.01); % Reduced pause, ensure overall loop time is reasonable for real-time feel
end

% Close serial port when done
clear device;
disp('Programme ended.');