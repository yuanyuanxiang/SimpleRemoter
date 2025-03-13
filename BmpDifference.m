% 计算客户端和服务端的位图差异
% MATLAB 8.1.0.604 (R2013a)
clear all;clc;
folder1=[pwd, '/client/bmp/']; % 客户端位图目录
folder2=[pwd, '/server/2015Remote/bmp/']; % 服务端位图目录

num1=numel(dir(fullfile(folder1, '*.bmp')));
disp([folder1, ' BMP file count: ', num2str(num1)]);
num2=numel(dir(fullfile(folder2, '*.bmp')));
disp([folder2, ' BMP file count: ', num2str(num2)]);
num = min(num1, num2);
missing = 0;
for i=1:num
    file1 = sprintf('%sGHOST_%d.bmp', folder1, i);
    file2 = sprintf('%sYAMA_%d.bmp', folder2, i);
    if exist(file2, 'file') == 2
        img1 = imread(file1);
        img2 = imread(file2);
        diff=double(img1)-double(img2);
        s = sum(diff(:));
        fprintf('BMP [%d] difference: %g\n', i, s);
    else
        fprintf('BMP [%d] difference: MISSING\n', i);
        missing = missing + 1;
    end
end

disp(missing);
