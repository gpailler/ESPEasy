//#######################################################################################################
//#################################### Plugin 035: Output IR ############################################
//#######################################################################################################
#ifdef PLUGIN_BUILD_NORMAL

#include <IRremoteESP8266.h>
IRsend *Plugin_035_irSender;

#define PLUGIN_035
#define PLUGIN_ID_035         35
#define PLUGIN_NAME_035       "Infrared Transmit"

uint8_t *base64_decode(String data, size_t *output_length);

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
            addLog(LOG_LEVEL_DEBUG, F("IRTX :IR DAIKIN"));
            // Serialized format arguments
            // ID TYPE          EXAMPLE
            // 1  Command       IRSEND
            // 2  IrType        DAIKIN_CUSTOM
            // 3  Hertz         34
            // 4  HeaderMark    5100
            // 5  HeaderSpace   2100
            // 6  OneMark       420
            // 7  OneSpace      1740
            // 8  ZeroMark      420
            // 9  ZeroSpace     620
            // 10 Data          EdoXGAQAHhHaFxgAcwAgHiggFgAgSQ==
            // 11 FrameSplitPos 7

            unsigned int IrHz = 0;
            unsigned int IrHeaderMark = 0;
            unsigned int IrHeaderSpace = 0;
            unsigned int IrOneMark = 0;
            unsigned int IrOneSpace = 0;
            unsigned int IrZeroMark = 0;
            unsigned int IrZeroSpace = 0;
            uint8_t *IrData;
            size_t IrDataLength;
            unsigned int IrFrameSplitPos;

            if (GetArgv(command, TmpStr1, 3)) IrHz = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 4)) IrHeaderMark = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 5)) IrHeaderSpace = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 6)) IrOneMark = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 7)) IrOneSpace = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 8)) IrZeroMark = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 9)) IrZeroSpace = str2int(TmpStr1);
            if (GetArgv(command, TmpStr1, 10)) IrData = base64_decode(TmpStr1, &IrDataLength);
            if (GetArgv(command, TmpStr1, 11)) IrFrameSplitPos = str2int(TmpStr1);

            // Debug dump
            addLog(LOG_LEVEL_DEBUG, String(F("Hz: ")) + IrHz);
            addLog(LOG_LEVEL_DEBUG, String(F("HeaderMark: ")) + IrHeaderMark);
            addLog(LOG_LEVEL_DEBUG, String(F("HeaderSpace: ")) + IrHeaderSpace);
            addLog(LOG_LEVEL_DEBUG, String(F("OneMark: ")) + IrOneMark);
            addLog(LOG_LEVEL_DEBUG, String(F("OneSpace: ")) + IrOneSpace);
            addLog(LOG_LEVEL_DEBUG, String(F("ZeroMark: ")) + IrZeroMark);
            addLog(LOG_LEVEL_DEBUG, String(F("ZeroSpace: ")) + IrZeroSpace);
            addLog(LOG_LEVEL_DEBUG, F("Data: "));
            char tmp[16];
            for (int i = 0; i < IrDataLength; i++)
            {
              sprintf(tmp, "0x%.2X", IrData[i]);
              addLog(LOG_LEVEL_DEBUG, tmp);
            }
            addLog(LOG_LEVEL_DEBUG, String(F("DataLength: ")) + IrDataLength);
            addLog(LOG_LEVEL_DEBUG, String(F("FrameSplitPos: ")) + IrFrameSplitPos);


            // Configure frequency
            Plugin_035_irSender->enableIROut(IrHz);

            // First frame
            sendDaikinHeader(IrHeaderMark, IrHeaderSpace);
            sendDaikinData(
              IrOneMark,
              IrOneSpace,
              IrZeroMark,
              IrZeroSpace,
              IrData,
              0,
              IrFrameSplitPos);

            // Delay between frames
            delay(29);

            // Second frame
            sendDaikinHeader(IrHeaderMark, IrHeaderSpace);
            sendDaikinData(
              IrOneMark,
              IrOneSpace,
              IrZeroMark,
              IrZeroSpace,
              IrData,
              IrFrameSplitPos,
              IrDataLength - IrFrameSplitPos);

            addLog(LOG_LEVEL_DEBUG, F("IRTX :IR DAIKIN SENT"));

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

void sendDaikinHeader(unsigned int headerMark, unsigned int headerSpace)
{
  Plugin_035_irSender->mark(headerMark);
  Plugin_035_irSender->space(headerSpace);
}

void sendDaikinData(
  unsigned int oneMark,
  unsigned int oneSpace,
  unsigned int zeroMark,
  unsigned int zeroSpace,
  uint8_t *data,
  size_t start,
  size_t length)
{
  int current;

  for (int i = start; i < start + length; i++)
  {
    current = data[i];

    for (int j = 0; j < 8; j++)
    {
      if ((1 << j & current))
      {
        Plugin_035_irSender->mark(oneMark);
        Plugin_035_irSender->space(oneSpace);
      }
      else
      {
        Plugin_035_irSender->mark(zeroMark);
        Plugin_035_irSender->space(zeroSpace);
      }
    }
  }

  Plugin_035_irSender->mark(oneMark);
  Plugin_035_irSender->space(zeroSpace);
}



// From https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
static int8_t encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static int8_t *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};

uint8_t *base64_decode(String data, size_t *output_length) {
    if (decoding_table == NULL) build_decoding_table();
    size_t input_length = data.length();

    if (input_length % 4 != 0) return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    uint8_t *decoded_data = static_cast<uint8_t*>(malloc(*output_length));
    if (decoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return decoded_data;
}

void build_decoding_table() {
    decoding_table = static_cast<int8_t*>(malloc(256));
    for (int i = 0; i < 64; i++) {
      decoding_table[(unsigned char) encoding_table[i]] = i;
    }
}

void base64_cleanup() {
    free(decoding_table);
}

#endif
