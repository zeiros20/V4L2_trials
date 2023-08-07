# Trial 2
The trial was to implememt the v4l2 api in c using my own understanding.

The program right now can be compiled and save images in jpeg format.

# HOW TO RUN

gcc -o cam trial_2.c interface.c

./cam <DEVICE NAME> <WIDTH> <HIEGHT> <NUMBER OF FRAMES>
i.e. "./cam /dev/video0 1280 720 5"
