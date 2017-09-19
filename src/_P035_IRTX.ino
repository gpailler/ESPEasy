//#######################################################################################################
//#################################### Plugin 035: Output IR ############################################
//#######################################################################################################
#ifdef PLUGIN_BUILD_NORMAL

#include <IRremoteESP8266.h>
IRsend *Plugin_035_irSender;

#define PLUGIN_035
#define PLUGIN_ID_035         35
#define PLUGIN_NAME_035       "Infrared Transmit"


#define DAIKIN_HDR_MARK      5100 //DAIKIN_ZERO_MARK*8
#define DAIKIN_HDR_SPACE  2100 //DAIKIN_ZERO_MARK*4
#define DAIKIN_ONE_SPACE  1740 
#define DAIKIN_ONE_MARK     420
#define DAIKIN_ZERO_MARK  420
#define DAIKIN_ZERO_SPACE   620

uint8_t get_checksum(uint8_t *data, uint8_t start, uint8_t end)
{
  uint8_t sum = 0;

  for(uint8_t i = start; i <= end; i++)
  {
    sum += data[i];
  }

  return sum & 0xFF;
}

void sendDaikin(unsigned char buf[], int len, int start)
{
  int data2;

  Plugin_035_irSender->enableIROut(34);
   
  Plugin_035_irSender->mark(DAIKIN_HDR_MARK);
  Plugin_035_irSender->space(DAIKIN_HDR_SPACE);
     
  for (int i = start; i < start+len; i++)
  {
    data2=buf[i];  
   
    for (int j = 0; j < 8; j++)
    {
      if ((1 << j & data2))
      {
        Plugin_035_irSender->mark(DAIKIN_ONE_MARK);
        Plugin_035_irSender->space(DAIKIN_ONE_SPACE);
      }
      else
      {
        Plugin_035_irSender->mark(DAIKIN_ZERO_MARK);
        Plugin_035_irSender->space(DAIKIN_ZERO_SPACE);
      }
    }
  }
  
  Plugin_035_irSender->mark(DAIKIN_ONE_MARK);
  Plugin_035_irSender->space(DAIKIN_ZERO_SPACE);
}

boolean Plugin_035(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_035;
        Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
        Device[deviceCount].SendDataOption = false;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_035);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        break;
      }

    case PLUGIN_INIT:
      {
        int irPin = Settings.TaskDevicePin1[event->TaskIndex];
        if (Plugin_035_irSender == 0 && irPin != -1)
        {
          addLog(LOG_LEVEL_INFO, F("INIT: IR TX"));
          Plugin_035_irSender = new IRsend(irPin);
          Plugin_035_irSender->begin(); // Start the sender
        }
        if (Plugin_035_irSender != 0 && irPin == -1)
        {
          addLog(LOG_LEVEL_INFO, F("INIT: IR TX Removed"));
          delete Plugin_035_irSender;
          Plugin_035_irSender = 0;
        }
        success = true;
        break;
      }

    case PLUGIN_WRITE:
      {
        String IrType;
        unsigned long IrCode;
        unsigned int IrBits;
        //char log[120];

        char command[120];
        command[0] = 0;
        char TmpStr1[100];
        TmpStr1[0] = 0;
        string.toCharArray(command, 120);

        String cmdCode = string;
        int argIndex = cmdCode.indexOf(',');
        if (argIndex) cmdCode = cmdCode.substring(0, argIndex);

        if (cmdCode.equalsIgnoreCase("IRSEND") && Plugin_035_irSender != 0)
        {
          success = true;
          #ifdef PLUGIN_016
          if (irReceiver != 0) irReceiver->disableIRIn(); // Stop the receiver
          #endif

          if (GetArgv(command, TmpStr1, 2)) IrType = TmpStr1;

          if (IrType.equalsIgnoreCase("RAW")) {
            String IrRaw;
            unsigned int IrHz=0;
            unsigned int IrPLen=0;
            unsigned int IrBLen=0;

            if (GetArgv(command, TmpStr1, 3)) IrRaw = TmpStr1;
            if (GetArgv(command, TmpStr1, 4)) IrHz = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 5)) IrPLen = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 6)) IrBLen = str2int(TmpStr1);

            printWebString += F("<a href='https://en.wikipedia.org/wiki/Base32#base32hex'>Base32Hex</a> RAW Code: ");
            printWebString += IrRaw;
            printWebString += F("<BR>");

            printWebString += F("kHz: ");
            printWebString += IrHz;
            printWebString += F("<BR>");

            printWebString += F("Pulse Len: ");
            printWebString += IrPLen;
            printWebString += F("<BR>");

            printWebString += F("Blank Len: ");
            printWebString += IrBLen;
            printWebString += F("<BR>");

            unsigned int buf[200];
            unsigned int idx = 0;
            unsigned int c0 = 0; //count consecutives 0s
            unsigned int c1 = 0; //count consecutives 1s

            printWebString += F("Interpreted RAW Code: ");
            //Loop throught every char in RAW string
            for(int i = 0; i < IrRaw.length(); i++)
            {
              //Get the decimal value from base32 table
              //See: https://en.wikipedia.org/wiki/Base32#base32hex
              char c = ((IrRaw[i] | ('A' ^ 'a')) - '0') % 39;

              //Loop through 5 LSB (bits 16, 8, 4, 2, 1)
              for (unsigned int shft = 1; shft < 6; shft++)
              {
                //if bit is 1 (5th position - 00010000 = 16)
                if ((c & 16) != 0) {
                  //add 1 to counter c1
                  c1++;
                  //if we already have any 0s in counting (the previous
                  //bit was 0)
                  if (c0 > 0) {
                    //add the total ms into the buffer (number of 0s multiplied
                    //by defined blank length ms)
                    buf[idx++] = c0 * IrBLen;
                    //print the number of 0s just for debuging/info purpouses
                    for (uint t = 0; t < c0; t++)
                      printWebString += F("0");
                  }
                  //So, as we recieve a "1", and processed the counted 0s
                  //sending them as a ms timing into the buffer, we clear
                  //the 0s counter
                  c0 = 0;
                } else {
                  //So, bit is 0

                  //On first call, ignore 0s (supress left-most 0s)
                  if (c0+c1 != 0) {
                    //add 1 to counter c0
                    c0++;
                    //if we already have any 1s in counting (the previous
                    //bit was 1)
                    if (c1 > 0) {
                      //add the total ms into the buffer (number of 1s
                      //multiplied by defined pulse length ms)
                      buf[idx++] = c1 * IrPLen;
                      //print the number of 1s just for debuging/info purpouses
                      for (uint t = 0; t < c1; t++)
                        printWebString += F("1");
                    }
                    //So, as we recieve a "0", and processed the counted 1s
                    //sending them as a ms timing into the buffer, we clear
                    //the 1s counter
                    c1 = 0;
                  }
                }
                //shift to left the "c" variable to process the next bit that is
                //in 5th position (00010000 = 16)
                c <<= 1;
              }
            }

            //Finally, we need to process the last counted bit that we were
            //processing

            //If we have pendings 0s
            if (c0 > 0) {
              buf[idx] = c0 * IrBLen;
              for (uint t = 0; t < c0; t++)
                printWebString += F("0");
            }
            //If we have pendings 1s
            if (c1 > 0) {
              buf[idx] = c1 * IrPLen;
              for (uint t = 0; t < c1; t++)
                printWebString += F("1");
            }

            printWebString += F("<BR>");

            Plugin_035_irSender->sendRaw(buf, idx+1, IrHz);

            //sprintf_P(log, PSTR("IR Params1: Hz:%u - PLen: %u - BLen: %u"), IrHz, IrPLen, IrBLen);
            //addLog(LOG_LEVEL_INFO, log);
            //sprintf_P(log, PSTR("IR Params2: RAW Code:%s"), IrRaw.c_str());
            //addLog(LOG_LEVEL_INFO, log);
          } else if(IrType.equalsIgnoreCase("DAIKIN")) {
            addLog(LOG_LEVEL_INFO, F("IRTX :IR DAIKIN Code Sent"));
            uint8_t data[] = {
              0x11, 0xDA, 0x17, 0x18, // 0-3   0-3    header
              0x04, 0x00,             // 4-5   4-5    unknown
              0x1E,                   // 6     5      checksum 
              0x11, 0xDA, 0x17, 0x18, // 7-10  0-3    header
              0x00,                   // 11    4      unknown
              0x73,                   // 12    5      mode/timer changed
              0x00,                   // 13    6      mode changed
              0x20,                   // 14    7      mode + on/off
              0x1E, 0x28,             // 15-16 8-9    timer
              0x20,                   // 17    10     temperature
              0x16,                   // 18    11     fan speed + swing mode 
              0x00, 0x20,             // 19-20 12-13  unknown
              0x49                    // 21    14     checksum
            };
            
            // On
            data[14] |= 0x01;
            
            // Off
            //data[14] &= 0xFE;
            
            data[6] = get_checksum(data, 0, 5);
            data[21] = get_checksum(data, 7, 20);
            
            sendDaikin(data, 7, 0); 
            delay(29);
            sendDaikin(data, 14, 7); 

          } else {
            if (GetArgv(command, TmpStr1, 2)) IrType = TmpStr1;
            if (GetArgv(command, TmpStr1, 3)) IrCode = strtoul(TmpStr1, NULL, 16); //(long) TmpStr1
            if (GetArgv(command, TmpStr1, 4)) IrBits = str2int(TmpStr1);

            if (IrType.equalsIgnoreCase("NEC")) Plugin_035_irSender->sendNEC(IrCode, IrBits);
            if (IrType.equalsIgnoreCase("JVC")) Plugin_035_irSender->sendJVC(IrCode, IrBits, 2);
            if (IrType.equalsIgnoreCase("RC5")) Plugin_035_irSender->sendRC5(IrCode, IrBits);
            if (IrType.equalsIgnoreCase("RC6")) Plugin_035_irSender->sendRC6(IrCode, IrBits);
            if (IrType.equalsIgnoreCase("SAMSUNG")) Plugin_035_irSender->sendSAMSUNG(IrCode, IrBits);
            if (IrType.equalsIgnoreCase("SONY")) Plugin_035_irSender->sendSony(IrCode, IrBits);
            if (IrType.equalsIgnoreCase("PANASONIC")) Plugin_035_irSender->sendPanasonic(IrBits, IrCode);
          }

          addLog(LOG_LEVEL_INFO, F("IRTX :IR Code Sent"));
          if (printToWeb)
          {
            printWebString += F("IR Code Sent ");
            printWebString += IrType;
            printWebString += F("<BR>");
          }

          #ifdef PLUGIN_016
          if (irReceiver != 0) irReceiver->enableIRIn(); // Start the receiver
          #endif
        }
        break;
      }
  }
  return success;
}

#endif
