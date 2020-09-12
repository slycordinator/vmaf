% Converts an image from the LAB2000HL color space to SRGB coordinates.
%
% For more information on the LAB2000HL color space, please see:
%           I. Lissner and P. Urban, "Toward a Unified Color Space
%           for Perception-Based Image Processing", IEEE Transactions
%           on Image Processing, Vol. 21, No. 3, pp. 1153-1168, 2012.
%
% This code is supplementary material to the article:
%       J. Preiss, F. Fernandes, and P. Urban, "Color-Image Quality 
%       Assessment: From Prediction to Optimization", IEEE Transactions on 
%       Image Processing, pp. 1366-1378, Volume 23, Issue 3, March 2014
%
% Authors:  Jens Preiss, Felipe Fernandes
%           Institute of Printing Science and Technology
%           Technische Universitšt Darmstadt
%           preiss.science@gmail.com
%           fernandes@idd.tu-darmstadt.de
%           http://www.idd.tu-darmstadt.de/color
%           and
%           Philipp Urban
%           Fraunhofer Institute for Computer Graphics Research IGD
%           philipp.urban@igd.fraunhofer.de
%           http://www.igd.fraunhofer.de/en/Institut/Abteilungen/3DT
%
% Interface:
%           IM_SRGB = ImageLAB2000HL2SRGB(IM_LAB2000HL)
%
% Parameters:
%           IM_LAB2000HL    MxNx3 matrix of LAB2000HL values
%           IM_SRGB         MxNx3 matrix of SRGB values
%
function IM_SRGB = ImageLAB2000HL2SRGB(IM_LAB2000HL)

    % Reshape the image before conversion
    [M, N, D] = size(IM_LAB2000HL);
    IM_LAB2000HL = reshape(IM_LAB2000HL, [M * N, D]);
    % Perform the LAB2000HL -> LAB transformation
    IM_LAB = LAB2000HL2LAB(IM_LAB2000HL);
    % Perform the LAB -> XYZ transformation
    IM_XYZ = LAB2XYZ(IM_LAB);
    % Perform the XYZ -> SRGB transformation
    IM_SRGB = XYZ2SRGB(IM_XYZ);
    % Reshape the image after the transformation
    IM_SRGB = reshape(IM_SRGB, [M, N, D]);
    
end