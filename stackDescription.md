
For the flight controller, we use a Teensy 4.1 with PlatformIO. The embedded code is written in C++, but we avoid `std` functionality that performs heap allocations. We mainly use C++ for features such as classes and namespaces. We generally try to avoid inheritance and prefer composition.

The drone also has a Jetson NX that runs programs written in C++ using the Qt framework, as well as Python and Rust. I initially started moving some of our software to Rust as a way to learn the language, but I now see a lot of benefits in using it.

Our RC (Remote Controller) also has a Teensy for processing pilot commands and sending them to the drone. The RC has a Jetson Nano as well, which runs the applications, including the UI.

We ship a laptop with the drone that handles log uploading and runs the CIC program. The CIC program handles and organizes the UT data received from the drone.
