/*
 * Copyright (c) 2009 Gerry Stellenberg, Adam Preble
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 *  pinproctest.cpp
 *  libpinproc
 */
#include "pinproctest.h"

PRMachineType machineType = kPRMachineInvalid;

/** Demonstration of the custom logging callback. */
void TestLogger(PRLogLevel level, const char *text)
{
    fprintf(stderr, "TEST: %s", text);
}

PRResult LoadConfiguration(YAML::Node& yamlDoc, const char *yamlFilePath)
{
    try
    {
        std::ifstream fin(yamlFilePath);
        if (fin.is_open() == false)
        {
            fprintf(stderr, "YAML file not found: %s\n", yamlFilePath);
            return kPRFailure;
        }
        
        yamlDoc = YAML::Load(fin);
    }
//    catch (YAML::ParserException& ex)
//    {
//        fprintf(stderr, "YAML parse error at line=%d col=%d: %s\n", ex.line, ex.column, ex.msg.c_str());
 //       return kPRFailure;
//    }
//    catch (YAML::RepresentationException& ex)
//    {
//        fprintf(stderr, "YAML representation error at line=%d col=%d: %s\n", ex.line, ex.column, ex.msg.c_str());
//        return kPRFailure;
//    }
    catch (...)
    {
        fprintf(stderr, "Unexpected exception while parsing YAML config.\n");
        return kPRFailure;
    }
    return kPRSuccess;
}

void ConfigureAccelerometerMotion(PRHandle proc)
{
    uint32_t readData[5];

    PRReadData(proc, 6, 0x10D, 1, readData); 
    printf("\nAccel chip id: %x\n", readData[0]);
    fflush(stdout);

    // Set FF_MT_COUNT (0x18)
    readData[0] = 1;
    PRWriteData(proc, 6, 0x118, 1, readData); 

    // Set FF_MT_THRESH (0x17)
    readData[0] = 1;
    PRWriteData(proc, 6, 0x117, 1, readData); 

    // Set FF_MT_CONFIG (0x15)
    readData[0] = 0xD8;
    PRWriteData(proc, 6, 0x115, 1, readData); 

    // Enable Motion interrupts
    readData[0] = 0x04;
    PRWriteData(proc, 6, 0x12D, 1, readData); 

    // Direct motion interrupt to int0 pin (default)
    readData[0] = 0x04;
    PRWriteData(proc, 6, 0x12E, 1, readData); 

    readData[0] = 0x3D;
    PRWriteData(proc, 6, 0x12A, 1, readData); 

    readData[0] = 0x02;
    PRWriteData(proc, 6, 0x12B, 1, readData); 


    // Enable auto-polling of accelerometer every 128 ms (8 times a sec).
    //readData[0] = 0x0F; // Enable polling, 8 times a second.
    readData[0] = 0x00;  // Disable polling
    readData[0] = readData[0] | 0x1600; // Set IRQ status addr (FF_MT_SRC)
    PRWriteData(proc, 6, 0x000, 1, readData); 
    PRFlushWriteData(proc);

}

void ConfigureAccelerometerTransient(PRHandle proc)
{
    uint32_t readData[5];

    PRReadData(proc, 6, 0x10D, 1, readData); 
    printf("\nAccel chip id: %x\n", readData[0]);
    fflush(stdout);

    // Set to standby so register changes will take.
    readData[0] = 0x0;
    PRWriteData(proc, 6, 0x12A, 1, readData); 

    // Set HPF_OUT bit in XYZ_DATA_CFG (0xOE)
    //readData[0] = 0x10;
    //PRWriteData(proc, 6, 0x10E, 1, readData); 


    // Set HP_FILTER_CUTOFF (0x0F)
    readData[0] = 0x03;
    PRWriteData(proc, 6, 0x10F, 1, readData); 

    // Set FF_TRANSIENT_COUNT (0x20)
    readData[0] = 1;
    PRWriteData(proc, 6, 0x120, 1, readData); 

    // Set FF_TRANSIENT_THRESH (0x1F)
    readData[0] = 1;
    PRWriteData(proc, 6, 0x11F, 1, readData); 

    // Set FF_TRANSIENT_CONFIG (0x1D)
    readData[0] = 0x1E;
    PRWriteData(proc, 6, 0x11D, 1, readData); 

    // Enable Motion interrupts
    readData[0] = 0x20;
    PRWriteData(proc, 6, 0x12D, 1, readData); 

    // Direct motion interrupt to int0 pin (default)
    readData[0] = 0x20;
    PRWriteData(proc, 6, 0x12E, 1, readData); 

    //readData[0] = 0x3D;
    readData[0] = 0x05;
    PRWriteData(proc, 6, 0x12A, 1, readData); 

    readData[0] = 0x02;
    PRWriteData(proc, 6, 0x12B, 1, readData); 


    // Enable auto-polling of accelerometer every 128 ms (8 times a sec).
    //readData[0] = 0x0F; // Enable polling, 8 times a second.
    readData[0] = 0x00;  // Disable polling
    readData[0] = readData[0] | 0x1E00; // Set IRQ status addr (FF_MT_SRC)
    PRWriteData(proc, 6, 0x000, 1, readData); 
    PRFlushWriteData(proc);

}

time_t startTime;
bool runLoopRun = true;

void RunLoop(PRHandle proc)
{
    const int maxEvents = 16;
    int i;
    PREvent events[maxEvents];
    uint32_t readData[5];
    // Create dot array using an array of bytes.  Each byte holds 8 dots.  Need
    // space for 4 sub-frames of 128/32 dots.
    unsigned char dots[4*((128*32)/8)]; 
    unsigned int dotOffset = 0;

    // Retrieve and store initial switch states. 
    LoadSwitchStates(proc);

    if (machineType != kPRMachineWPCAlphanumeric) {
      // Send 3 frames
      for (i=0; i<3; i++)
      {
        // Create a dot pattern to test the DMD
        UpdateDots(dots,dotOffset++);
        PRDMDDraw(proc,dots);
      }
    }

    //ConfigureAccelerometerMotion(proc);
    ConfigureAccelerometerTransient(proc);

    int p = 0;

    while (runLoopRun)
    {
        PRDriverWatchdogTickle(proc);

//        PRReadData(proc, 6, 0x115, 1, readData); 
//        printf("\n\n\nAccel chip id: %x\n", readData[0]);
//        PRReadData(proc, 6, 0x116, 1, readData); 
//        printf("\nAccel chip id: %x\n", readData[0]);
//        PRReadData(proc, 6, 0x117, 1, readData); 
//        printf("\nAccel chip id: %x\n", readData[0]);
//        PRReadData(proc, 6, 0x118, 1, readData); 
//        printf("\nAccel chip id: %x\n", readData[0]);
//        PRReadData(proc, 6, 0x12D, 1, readData); 
//        printf("\nAccel chip id: %x\n", readData[0]);
//        PRReadData(proc, 6, 0x12E, 1, readData); 
//        printf("\nAccel chip id: %x\n", readData[0]);

        int numEvents = PRGetEvents(proc, events, maxEvents);
//        if (numEvents > 0) printf("\nNum events: %x\n", numEvents);
        for (int i = 0; i < numEvents; i++)
        {
            PREvent *event = &events[i];
            const char *stateText = "Unknown";
            switch (event->type)
            {
                case kPREventTypeSwitchOpenDebounced: stateText = "open"; break;
                case kPREventTypeSwitchClosedDebounced: stateText = "closed"; break;
                case kPREventTypeSwitchOpenNondebounced: stateText = "open(ndb)"; break;
                case kPREventTypeSwitchClosedNondebounced: stateText = "closed(ndb)"; break;
            }
#ifdef _MSC_VER
            struct _timeb tv;
            _ftime(&tv);
#else
            struct timeval tv;
            gettimeofday(&tv, NULL);
#endif
            switch (event->type)
            {
                case kPREventTypeSwitchOpenDebounced: 
                case kPREventTypeSwitchClosedDebounced:
                case kPREventTypeSwitchOpenNondebounced:
                case kPREventTypeSwitchClosedNondebounced:
                {
#ifdef _MSC_VER
                    printf("%d.%03d switch % 3d: %s\n", tv.time-startTime, tv.millitm, event->value, stateText);
#else
                    printf("%d.%03d switch % 3d: %s\n", (int)(tv.tv_sec-startTime), (int)tv.tv_usec/1000, event->value, stateText);
#endif
                    UpdateSwitchState( event );
                    break;
                }
                case kPREventTypeDMDFrameDisplayed:
                {
                    if (machineType == kPRMachineWPCAlphanumeric) {
                        //UpdateAlphaDisplay(proc, dotOffset++);
                    }
                    else {
                        UpdateDots(dots,dotOffset++);
                        PRDMDDraw(proc,dots);
                    }
                    break;
                }
                case kPREventTypeAccelerometerX:
                {
                    //readData[0] = event->value & 0x3FFF;
                    readData[0] = event->value;
                    break;
                }
                case kPREventTypeAccelerometerY:
                {
                    //readData[1] = event->value & 0x3FFF;
                    readData[1] = event->value;
                    break;
                }
                case kPREventTypeAccelerometerZ:
                {
                    //readData[2] = event->value & 0x3FFF;
                    readData[2] = event->value;
                    printf("\nAccel: X: %x, Y: %x, Z: %x", readData[0], readData[1],readData[2]);
                    break;
                }
                case kPREventTypeAccelerometerIRQ:
                {
                    //readData[2] = event->value & 0x3FFF;
                    readData[3] = event->value;
                    printf("\nAccel IRQ: %x", readData[3]);
                    break;
                }
                default:
                {
                    printf("\nUnknown event: %x:%x", event->type, event->value);
                }
            }
        }
        PRFlushWriteData(proc);
#ifdef _MSC_VER
        Sleep(10);
#else
        usleep(10*1000); // Sleep for 10ms so we aren't pegging the CPU.
#endif
    }
}

void sigint(int)
{
    runLoopRun = false;
    signal(SIGINT, SIG_DFL); // Re-install the default signal handler.
    printf("Exiting...\n");
}

int main(int argc, const char **argv)
{
    int i;

    // Set a signal handler so that we can exit gracefully on Ctrl-C:
    signal(SIGINT, sigint);
    startTime = time(NULL);

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <yaml machine description>\n", argv[0]);
        return 1;
    }
    const char *yamlFilename = argv[1];
    
    // Assign a custom logging callback to demonstrate capturing log information from P-ROC:
    PRLogSetCallback(TestLogger);
    
    YAML::Node yamlDoc;
    if (LoadConfiguration(yamlDoc, yamlFilename) != kPRSuccess)
    {
        fprintf(stderr, "Failed to load configuration file %s\n", yamlFilename);
        return 1;
    }

    std::string machineTypeString = yamlDoc["PRGame"]["machineType"].as<std::string>();
    if (machineTypeString == "wpc")
        machineType = kPRMachineWPC;
    else if (machineTypeString == "wpc95")
        machineType = kPRMachineWPC95;
    else if (machineTypeString == "wpcAlphanumeric")
        machineType = kPRMachineWPCAlphanumeric;
    else if(machineTypeString == "sternWhitestar")
        machineType = kPRMachineSternWhitestar;
    else if(machineTypeString == "sternSAM")
        machineType = kPRMachineSternSAM;
    else if(machineTypeString == "custom")
        machineType = kPRMachineCustom;
    else
    {
        fprintf(stderr, "Unknown machine type: %s\n", machineTypeString.c_str());
        return 1;
    }

    // Finally instantiate the P-ROC device:
    PRHandle proc = PRCreate(machineType);
    if (proc == kPRHandleInvalid)
    {
        fprintf(stderr, "Error during PRCreate: %s\n", PRGetLastErrorText());
        return 1;
    }

    PRLogSetLevel (kPRLogInfo);
    PRReset(proc, kPRResetFlagUpdateDevice); // Reset the device structs and write them into the device.
    
    // Even if WPCAlphanumeric, configure the DMD at least to get frame events for
    // timing purposes.
    ConfigureDMD(proc); 
    if (machineType == kPRMachineCustom) ConfigureDrivers(proc);
    ConfigureSwitches(proc, yamlDoc); // Notify host for all debounced switch events.
    ConfigureSwitchRules(proc, yamlDoc); // Flippers, slingshots

    if (machineType == kPRMachineWPCAlphanumeric) UpdateAlphaDisplay(proc, 0);

    // Pulse a coil for testing purposes.
    PRDriverPulse(proc, 47, 30);
    // Schedule a feature lamp for testing purposes.
    for (i=0; i<8; i++) {
      PRDriverSchedule(proc, 80+i, 0xFF00FF00, 0, 0);
    }

    //PRDriverSchedule(proc, 80, 0xFF00FF00, 0, 0);
    //PRDriverSchedule(proc, 0, 0xFF00AAAA, 1, 1);
    
    // Pitter-patter lamp 84: on 127ms, off 127ms, forever.
    //PRDriverPatter(proc, 84, 127, 127, 0);
    
    //Pulsed Patter for coil 48: on 5ms, off 10ms, repeat for 45ms.
    //PRDriverPulsedPatter(proc, 48, 5, 10, 45); // Coil 48: on 5ms, off 10ms, repeat for 45ms.

/*
    PRDriverAuxCommand auxCommands[256];

    // Disable the first entry so the Aux logic won't begin immediately.
    PRDriverAuxPrepareDisable(&auxCommands[0]);
    // Set up a sequence of outputs.
    for (i=0; i<16; i++) {
      PRDriverAuxPrepareOutput(&(auxCommands[i+1]), i, 0, 8, false);
    }
    // Disable the last command so the sequence stops.
    // PRDriverAuxPrepareDisable(&auxCommands[17]);
    // Jump from addr 17 to 1 to repeat.
    PRDriverAuxPrepareDelay(&auxCommands[17],1000);
    PRDriverAuxPrepareJump(&auxCommands[18],1);

    // Send the commands.
    PRDriverAuxSendCommands(proc, auxCommands, 19, 0);

    // Jump from addr 0 to 1 to begin.
    PRDriverAuxPrepareJump(&auxCommands[0],1);
    PRDriverAuxSendCommands(proc, auxCommands, 1, 0);
*/
    PRFlushWriteData(proc);


    printf("Running.  Hit Ctrl-C to exit.\n");
    
    RunLoop(proc);

    // Clean up P-ROC.
    printf("Disabling P-ROC drivers and switch rules...\n");

    PRReset(proc, kPRResetFlagUpdateDevice); // Reset the device structs and write them into the device.
    PRFlushWriteData(proc);

    // Destroy the P-ROC device handle:
    PRDelete(proc);
    proc = kPRHandleInvalid;
    return 0;
}
