/*
 * gprs.cpp
 * A library for SeeedStudio seeeduino GPRS shield
 *
 * Copyright (c) 2013 seeed technology inc.
 * Author        :   lawliet zou
 * Create Time   :   Dec 2013
 * Change Log    :
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "gprs.h"

void *memchr(const void *s, int c, size_t n) {
  unsigned char *p = (unsigned char *)s;
  while (n--)
    if (*p != (unsigned char)c)
      p++;
    else
      return p;
  return 0;
}

int GPRS::init(void) {
#if 0
    for(int i = 0; i < 2; i++){
        sendCmd("AT\r\n");
        delay(100);
    }
    sendCmd("AT+CFUN=1\r\n");
    if(0 != checkSIMStatus()) {
        ERROR("ERROR:checkSIMStatus");
        return -1;
    }
    return 0;

#endif
  if (sendCmdAndWaitForResp("AT\r\n", "OK\r\n", DEFAULT_TIMEOUT * 3)) {
    return -1;
  }
  if (sendCmdAndWaitForResp("AT+CFUN=1\r\n", "OK\r\n", DEFAULT_TIMEOUT * 3)) {
    return -1;
  }
  if (checkSIMStatus()) {
    return -1;
  }
  return 0;
}

bool GPRS::join(const char *apn, const char *userName, const char *passWord) {
  // char cmd[64];
  char ipAddr[32];
  // char gprsBuffer[32];

  // Select multiple connection
  // sim900_check_with_cmd("AT+CIPMUX=1\r\n","OK",DEFAULT_TIMEOUT,CMD);

  cleanBuffer(ipAddr, 32);
  sendCmd("AT+CIFSR\r\n");
  readBuffer(ipAddr, 32, 2);

  // If no IP address feedback than bring up wireless
  if (NULL != strstr(ipAddr, "ERROR")) {
    if (0 != sendCmdAndWaitForResp("AT+CSTT?\r\n", apn, DEFAULT_TIMEOUT)) {
      sendCmd("AT+CSTT=\"");
      sendCmd(apn);
      sendCmd("\",\"");
      sendCmd(userName);
      sendCmd("\",\"");
      sendCmd(passWord);
      sendCmdAndWaitForResp("\"\r\n", "OK\r\n", DEFAULT_TIMEOUT * 3);
    }

    // Brings up wireless connection
    sendCmd("AT+CIICR\r\n");

    // Get local IP address
    cleanBuffer(ipAddr, 32);
    sendCmd("AT+CIFSR\r\n");
    readBuffer(ipAddr, 32, 2);
  }
#if 0
    Serial.print("ipAddr: ");
    Serial.println(ipAddr);
#endif

  if (NULL != strstr(ipAddr, "AT+CIFSR")) {
    _ip = str_to_ip(ipAddr + 11);
    if (_ip != 0) {
      return true;
    }
  }
  return false;
}

uint32_t GPRS::str_to_ip(const char *str) {
  uint32_t ip = 0;
  char *p = (char *)str;

  for (int i = 0; i < 4; i++) {
    ip |= atoi(p);
    p = strchr(p, '.');
    if (p == NULL) {
      break;
    }
    if (i < 3)
      ip <<= 8;
    p++;
  }
  return ip;
}

// HACERR lo de la IP gasta muuuucho espacio (ver .h y todo esto)
char *GPRS::getIPAddress() {
  uint8_t a = (_ip >> 24) & 0xff;
  uint8_t b = (_ip >> 16) & 0xff;
  uint8_t c = (_ip >> 8) & 0xff;
  uint8_t d = _ip & 0xff;

  snprintf(ip_string, sizeof(ip_string), "%d.%d.%d.%d", a, b, c, d);
  return ip_string;
}

int GPRS::checkSIMStatus(void) {
  char gprsBuffer[32];
  int count = 0;
  cleanBuffer(gprsBuffer, 32);
  while (count < 3) {
    sendCmd("AT+CPIN?\r\n");
    readBuffer(gprsBuffer, 32, DEFAULT_TIMEOUT);
    if ((NULL != strstr(gprsBuffer, "+CPIN: READY"))) {
      break;
    }
    count++;
    delay(300);
    yield();
  }
  if (count == 3) {
    return -1;
  }
  return 0;
}

int GPRS::networkCheck(void) {
  delay(1000);
  if (0 != sendCmdAndWaitForResp("AT+CGREG?\r\n", "+CGREG: 0,1",
                                 DEFAULT_TIMEOUT * 3)) {
    ERROR("ERROR:CGREG");
    return -1;
  }
  delay(1000);
  if (0 !=
      sendCmdAndWaitForResp("AT+CGATT?\r\n", "+CGATT: 1", DEFAULT_TIMEOUT)) {
    ERROR("ERROR:CGATT");
    return -1;
  }
  return 0;
}

int GPRS::sendSMS(char *number, char *data) {
  char cmd[32];
  if (0 !=
      sendCmdAndWaitForResp("AT+CMGF=1\r\n", "OK",
                            DEFAULT_TIMEOUT)) { // Set message mode to ASCII
    ERROR("ERROR:CMGF");
    return -1;
  }
  delay(500);
  snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r\n", number);
  if (0 != sendCmdAndWaitForResp(cmd, ">", DEFAULT_TIMEOUT)) {
    ERROR("ERROR:CMGS");
    return -1;
  }
  delay(1000);
  serialSIM800.write(data);
  delay(500);
  sendEndMark();
  return 0;
}

int GPRS::readSMS(int messageIndex, char *message, int length) {
  int i = 0;
  char gprsBuffer[100];
  char cmd[16];
  char *p, *s;

  sendCmdAndWaitForResp("AT+CMGF=1\r\n", "OK", DEFAULT_TIMEOUT);
  delay(1000);
  sprintf(cmd, "AT+CMGR=%d\r\n", messageIndex);
  serialSIM800.write(cmd);
  cleanBuffer(gprsBuffer, 100);
  readBuffer(gprsBuffer, 100, DEFAULT_TIMEOUT);

  if (NULL != (s = strstr(gprsBuffer, "+CMGR"))) {
    if (NULL != (s = strstr(gprsBuffer, "+32"))) {
      p = s + 6;
      while ((*p != '$') && (i < length - 1)) {
        message[i++] = *(p++);
      }
      message[i] = '\0';
    }
  }
  return 0;
}

int GPRS::deleteSMS(int index) {
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "AT+CMGD=%d\r\n", index);
  sendCmd(cmd);
  return 0;
}

int GPRS::callUp(char *number) {
  char cmd[24];
  if (0 != sendCmdAndWaitForResp("AT+COLP=1\r\n", "OK", 5)) {
    ERROR("COLP");
    return -1;
  }
  delay(1000);
  sprintf(cmd, "\r\nATD%s;\r\n", number);
  serialSIM800.write(cmd);
  return 0;
}

int GPRS::answer(void) {
  serialSIM800.write("ATA\r\n");
  return 0;
}

int GPRS::connectTCP(const char *ip, int port) {
  char cipstart[50];
  sprintf(cipstart, "AT+CIPSTART=\"TCP\",\"%s\",\"%d\"\r\n", ip, port);
  if (0 != sendCmdAndWaitForResp(cipstart, "CONNECT OK",
                                 2 * DEFAULT_TIMEOUT)) { // connect tcp
    ERROR("ERROR:CIPSTART");
    return -1;
  }

  return 0;
}
int GPRS::sendTCPData(char *data) {
  char cmd[32];
  int len = strlen(data);
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", len);
  if (0 != sendCmdAndWaitForResp(cmd, ">", 2 * DEFAULT_TIMEOUT)) {
    ERROR("ERROR:CIPSEND");
    return -1;
  }

  if (0 != sendCmdAndWaitForResp(data, "SEND OK", 2 * DEFAULT_TIMEOUT)) {
    ERROR("ERROR:SendTCPData");
    return -1;
  }
  return 0;
}

int GPRS::closeTCP(void) {
  sendCmd("AT+CIPCLOSE\r\n");
  return 0;
}

int GPRS::shutTCP(void) {
  sendCmd("AT+CIPSHUT\r\n");
  return 0;
}

/**
 * setup modem
 * @method GPRS::modemSetup
 * @param  isWaitingDevice  0=skip if fail or ok, 1=wait until modem reply
 * @return                  0=ok, else = error
 */
int GPRS::gprsModemSetup(const uint8_t isWaitingDevice) {
  char buffer[40] = {0};
  int resx = -1;

  // modem is up
  while ((resx = sendCmdAndWaitForResp("AT\r\n", "OK", 2000, sizeof(buffer),
                                       buffer)) != 0) {
    if (!isWaitingDevice) {
      break;
    } else {
      yield();
      delay(10);
    }
  }

  if (resx == 0) {
    // no command echo
    if ((resx = sendCmdAndWaitForResp("ATE0\r\n", "OK", 5000, sizeof(buffer),
                                      buffer)) == 0) {
      // set to text mode
      if ((resx = sendCmdAndWaitForResp("AT+CMGF=1\r\n", "OK", 5000,
                                        sizeof(buffer), buffer)) == 0) {
        // set storage to simcard
        if ((resx =
                 sendCmdAndWaitForResp("AT+CPMS=\"SM\",\"SM\",\"SM\"\r\n", "OK",
                                       5000, sizeof(buffer), buffer)) == 0) {
          // clip=1
          if ((resx = sendCmdAndWaitForResp("AT+CLIP=1\r\n", "OK", 5000,
                                            sizeof(buffer), buffer)) == 0) {
          } else {
            // clip error
            resx += 40;
          }
        } else {
          // set storage error
          resx += 30;
        }
      } else {
        // set text mode error
        resx += 20;
      }
    } else {
      // ate0 error
      resx += 10;
    }
  }

  return resx;
}

/**
 * send sms
 * @method GPRS::gprsSendSms
 * @param  aSenderNumber     number
 * @param  aMessage          messages
 * @return                   0=OK, else=error
 */
int GPRS::gprsSendSms(const char *aSenderNumber, const char *aMessage) {
  const uint8_t MAX_BUFFER = 128;
  int respon = 0;
  char buffer[16] = {0};
  char *cmdx = (char *)calloc(MAX_BUFFER, sizeof(char));

  // buffer created???
  if (cmdx == NULL) {
    return -1;
  }

  // build the string
  memset(cmdx, '\0', MAX_BUFFER);
  snprintf((char *)cmdx, MAX_BUFFER, "AT+CMGS=\"%s\"\r", aSenderNumber);

  if ((respon = sendCmdAndWaitForResp(cmdx, ">", 2000, sizeof(buffer),
                                      buffer)) == 0) {
    delay(100);

    // only copy MAX_BUFFER-2 char max from msg
    memcpy(&cmdx[0], aMessage, MAX_BUFFER - 2);

    // cmdx[MAX_BUFFER - 2] = 0x1a;
    // cmdx[MAX_BUFFER - 1] = 0x00;
    cmdx[strlen(cmdx)] = 0x1a;

    if ((respon = sendCmdAndWaitForResp(cmdx, "OK", 30000, sizeof(buffer),
                                        buffer)) == 0) {
    }
  }

  // free buffer
  free(cmdx);

  return respon;
}

/**
 * get signal quality
 * @method GPRS::getSignalQuality
 * @return 0=ok, else error
 */
int GPRS::getSignalQuality() {
  char buffer[40] = {0}, tt[10] = {0};
  int resx = -1;
  char *sx, *nx;

  resx =
      sendCmdAndWaitForResp("AT+CSQ\r\n", "OK", 1000, sizeof(buffer), buffer);

  if (resx == 0) {
    if ((sx = strstr(buffer, "+CSQ:")) != NULL) {
      // search \n position
      nx = strchr(sx + 6, '\n');
      if (nx != NULL) {
        // copy the data csq
        strncpy(tt, sx + 6, nx - sx - 6);

        // split it, only get 2 data
        sx = strtok(tt, ",");
        if (sx != NULL) {
          rssi = atoi(sx);
          rssi_normalize = map(rssi, 0, 31, 0, 5);

          sx = strtok(NULL, ",");
          if (sx != NULL) {
            ber = atoi(sx);
          }
        }
      }
    }
  }

  sx = nx = NULL;

  return resx;
}

// gprs command new
/**
 * Check if networ is registered
 * @method GPRS::gprsCheckNetworkIsRegistered
 * @return 0=OK, -1=ERROR
 */
int GPRS::gprsCheckNetworkIsRegistered() {
  // char buffx[64] = {0};

  if (sendCmdAndWaitForResp("AT+CREG?\r\n", "0,1", 5000, 0, NULL) != 0) {
    // ERROR(buffx);
    if (sendCmdAndWaitForResp("AT+CREG?\r\n", "0,5", 5000, 0, NULL) != 0) {
      // ERROR(buffx);
      return -1;
    }
  }
  return 0;
}

/**
 * gprs check if it is already connected
 * @method GPRS::gprsCheckIfAlreadyOpened
 * @return 0=OK, -1=ERROR
 */
int GPRS::gprsCheckIfAlreadyOpened() {
  return sendCmdAndWaitForResp("AT+SAPBR=2,1\r\n", "1,1", 5000, 0, NULL);
}

/**
 * gprs open connection
 * @method GPRS::gprsOpenConnection
 * @param  apn                      APN name
 * @param  user                     username
 * @param  pwd                      password
 * @return                          0=OK, -1.. = ERROR
 */
int GPRS::gprsOpenConnection(const char *apn, const char *user,
                             const char *pwd) {
#define CMD_SAPBR_NEW "AT+SAPBR=3,1,"

  const uint8_t MAX_BUFFER_NUM = 64;
  // uint8_t cmdx[MAX_BUFFER_NUM];
  char *cmdx = (char *)calloc(MAX_BUFFER_NUM, sizeof(char));

  // buffer created??
  if (cmdx == NULL) {
    return -1;
  }

  // setup string
  memset(cmdx, '\0', MAX_BUFFER_NUM);

  // check network connection
  if (gprsCheckNetworkIsRegistered() != 0) {
    free(cmdx);
    return -2;
  }

  if (gprsCheckIfAlreadyOpened() == 0) {
    free(cmdx);
    return 0;
  }

  // init gprs connection
  snprintf((char *)cmdx, MAX_BUFFER_NUM, "%s\"CONTYPE\",\"GPRS\"\r\n",
           CMD_SAPBR_NEW);
  if (sendCmdAndWaitForResp(cmdx, "OK", 5000, 0, NULL) != 0) {
    free(cmdx);
    return -3;
  }

  // set apn
  snprintf((char *)cmdx, MAX_BUFFER_NUM, "%s\"APN\",\"%s\"\r\n", CMD_SAPBR_NEW,
           apn);
  if (sendCmdAndWaitForResp(cmdx, "OK", 5000, 0, NULL) != 0) {
    free(cmdx);
    return -4;
  }

  // set username
  snprintf((char *)cmdx, MAX_BUFFER_NUM, "%s\"USER\",\"%s\"\r\n", CMD_SAPBR_NEW,
           user);
  if (sendCmdAndWaitForResp(cmdx, "OK", 5000, 0, NULL) != 0) {
    free(cmdx);
    return -5;
  }

  // set passwd
  snprintf((char *)cmdx, MAX_BUFFER_NUM, "%s\"PWD\",\"%s\"\r\n", CMD_SAPBR_NEW,
           pwd);
  if (sendCmdAndWaitForResp(cmdx, "OK", 5000, 0, NULL) != 0) {
    free(cmdx);
    return -6;
  }

  // start gprs connection
  if (sendCmdAndWaitForResp("AT+SAPBR=1,1\r\n", "OK", 60000, 0, NULL) != 0) {
    free(cmdx);
    return -7;
  }

  free(cmdx);
  return 0;
}

/**
 * Close gprs connection
 * @method GPRSS::gprsCloseConnection
 * @return 0=OK, -1=ERROR
 */
int GPRS::gprsCloseConnection() {
  return sendCmdAndWaitForResp("AT+SAPBR=0,1\r\n", "OK", 30000, 0, NULL);
}

/**
 * Terminate http connection to server
 * @method GPRS::gprsTerminateHTTPConnection
 * @return 0=OK, -1=ERROR
 */
int GPRS::gprsTerminateHTTPConnection() {
  return sendCmdAndWaitForResp("AT+HTTPTERM\r\n", "OK", 5000, 0, NULL);
}

/**
 * Send data to server with post or get method
 * @method GPRS::gprsSendData
 * @param  method             HTTP_MODEM_POST or HTTP_MODEM_GET
 * @param  aurl               complete server script url
 * @param  adata              data to be sent
 * @param  max_out_len        max length of buffer output
 * @param  arespon_out        the buffer output
 * @return                    0=OK, -1... = ERROR
 */
int GPRS::gprsSendData(const uint8_t method, const char *aurl,
                       const char *adata, const uint8_t max_out_len,
                       char *arespon_out) {

#define HTTP_PARA_URL "AT+HTTPPARA=\"URL\","
  const uint8_t MAX_BUFFER = 128;
  int respon = 0;
  uint16_t num_data = 0;
  char *cmdx = (char *)calloc(MAX_BUFFER, sizeof(char));

  // buffer created?
  if (cmdx == NULL) {
    return -1;
  }

  // set init result
  if (arespon_out != NULL) {
    memset(arespon_out, '\0', max_out_len * sizeof(char));
  }

  // check is connected
  if (gprsCheckIfAlreadyOpened() != 0) {
    free(cmdx);
    return -2;
  }

  // make sure previous http is terminated
  gprsTerminateHTTPConnection();

  // init http
  delay(100);
  if (sendCmdAndWaitForResp("AT+HTTPINIT\r\n", "OK", 5000, 0, NULL) != 0) {
    free(cmdx);
    return -3;
  }

  // http parameter cid
  if (sendCmdAndWaitForResp("AT+HTTPPARA=\"CID\",1\r\n", "OK", 5000, 0, NULL) !=
      0) {
    free(cmdx);
    return -4;
  }

  // http parameter url
  memset(cmdx, '\0', MAX_BUFFER);
  if (method == HTTP_MODEM_POST) // post
  {
    snprintf((char *)cmdx, MAX_BUFFER, "%s\"%s\"\r\n", HTTP_PARA_URL, aurl);
  } else // get
  {
    snprintf((char *)cmdx, MAX_BUFFER, "%s\"%s?%s\"\r\n", HTTP_PARA_URL, aurl,
             adata);
  }
  if (sendCmdAndWaitForResp(cmdx, "OK", 5000, 0, NULL) != 0) {
    free(cmdx);
    gprsTerminateHTTPConnection();
    return -5;
  }

  // set content type for post only
  if (method == HTTP_MODEM_POST) {
    if (sendCmdAndWaitForResp(
            "AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"\r\n",
            "OK", 5000, 0, NULL) != 0) {
      free(cmdx);
      gprsTerminateHTTPConnection();
      return -6;
    }

    // http post data
    memset(cmdx, '\0', MAX_BUFFER);
    snprintf((char *)cmdx, MAX_BUFFER, "AT+HTTPDATA=%d,20000\r\n",
             strlen((const char *)adata));
    if (sendCmdAndWaitForResp(cmdx, "DOWNLOAD", 30000, 0, NULL) != 0) {
      free(cmdx);
      gprsTerminateHTTPConnection();
      return -7;
    }

    // send the data
    if (sendCmdAndWaitForResp(adata, "DOWNLOAD", 30000, 0, NULL) != 0) {
      free(cmdx);
      gprsTerminateHTTPConnection();
      return -8;
    }
  }

  // send it
  memset(cmdx, '\0', MAX_BUFFER);
  respon = sendCmdAndWaitForResp(
      ((method == HTTP_MODEM_POST) ? "AT+HTTPACTION=1\r\n"
                                   : "AT+HTTPACTION=0\r\n"),
      ((method == HTTP_MODEM_POST) ? "+HTTPACTION: 1,200"
                                   : "+HTTPACTION: 0,200"),
      30000, sizeof(cmdx), cmdx);

  // sendCmd(
  //     ((method == HTTP_MODEM_POST) ? "AT+HTTPACTION=1\r\n" :
  //     "AT+HTTPACTION=0\r\n"));
  //
  // // send success??
  // respon = 0;
  if (respon == 0) {

    // respon = readBuffer(cmdx, MAX_BUFFER, 8 * DEFAULT_TIMEOUT);

    if ((strstr(cmdx, ((method == HTTP_MODEM_POST) ? "+HTTPACTION: 1,200"
                                                   : "+HTTPACTION: 0,200")) !=
         NULL)) {
      // find \r on the data respons
      // cmdx form = ,[respon length]\r
      char *pr = (char *)memchr(cmdx, '$', MAX_BUFFER);

      num_data = 0;
      if (pr != NULL) {
        uint8_t clen[6];
        memset(clen, '\0', 6);                 // set all to 0
        memcpy(clen, &cmdx[1], pr - &cmdx[1]); // copy the data respon length
        num_data = atoi((const char *)clen);   // convert to int
      }

      // just limit it
      if (num_data > max_out_len) {
        num_data = max_out_len;
      }

      // read the data respon
      if (num_data > 0) {
        // allocate memory
        // make sure a respon out is capable of receiveing it
        // beware that you must make sure that the usart rx buffer is capable of
        // receiving it
        memset(cmdx, '\0', MAX_BUFFER);
        snprintf((char *)cmdx, MAX_BUFFER, "AT+HTTPREAD=0,%d\r\n", num_data);

        // respon = sendCmdAndWaitForResp(cmdx, "+HTTPREAD:",
        // 2*DEFAULT_TIMEOUT);
        sendCmd(cmdx);
        respon = 0;
        if (respon == 0) {
          // get the rest of data
          memset(cmdx, '\0', MAX_BUFFER);
          respon = readBuffer(cmdx, MAX_BUFFER, 2);

          if (respon == 0) {
            // find first \n position
            pr = (char *)memchr(cmdx, '$', num_data * sizeof(char));

            // copy to result
            memcpy(arespon_out, pr + 1, num_data * sizeof(char));
          }
        }
      }
    }
  }

  // free buffer
  free(cmdx);

  // terminate http
  gprsTerminateHTTPConnection();

  return 0;
}

/**
 * Send command to modem and wait for response
 * @method GPRS::modemSendCommandWaitReply
 * @param  aCmd                            the command
 * @param  aResponExit                     the exit response
 * @param  aTimeoutMax                     max timeout in ms
 * @param  aLenOut                         max buffer out length
 * @param  aResponOut                      buffer output pointer
 * @return                                 0=ok, 1=error, <0 = failure
 */
// int GPRS::modemSendCommandWaitReply(const char *aCmd, const char
// *aResponExit,
//                                     const uint32_t aTimeoutMax,
//                                     const char aLenOut, char *aResponOut) {
//
// #define MAX_BUFFER_TMP 128
//
//   int id_data, respons = 0;
//   uint32_t uart_tout_cnt = 0;
//   // uint16_t uart_data;
//
//   char *aDataBuffer = (char *)calloc(MAX_BUFFER_TMP, sizeof(char));
//
//   // buffer created???
//   if (aDataBuffer == NULL) {
//     return -1;
//   }
//
//   // reset to all 0
//   memset(aDataBuffer, '\0', MAX_BUFFER_TMP);
//
//   // read left buffer data
//   // discard all received data, if any
//   while (serialSIM800.available() > 0) {
//     serialSIM800.read();
//     yield();
//   }
//
//   // send command
//   if (aCmd != NULL) {
//     serialSIM800.write(aCmd);
//   }
//
//   // wait for reply
//   id_data = 0;
//   uart_tout_cnt = 0;
//   while ((id_data < (MAX_BUFFER_TMP - 1)) && (uart_tout_cnt <= aTimeoutMax))
//   {
//     // get uart data or timeout
//     uart_tout_cnt = 0;
//     while ((serialSIM800.available() == 0) && (uart_tout_cnt < aTimeoutMax))
//     // wait data arrive or tout
//     {
//       uart_tout_cnt++;
//       yield();
//       delay(1);
//     }
//
//     // check for timeout
//     if (uart_tout_cnt >= aTimeoutMax) {
//       respons = -2;
//       break;
//     } else {
//       aDataBuffer[id_data] = (char)serialSIM800.read();
//       id_data++;
//
//       // check if the desired answer  is in the response of the module
//       if (aResponExit != NULL) {
//         if (strstr((const char *)aDataBuffer, (const char *)aResponExit) !=
//             NULL) {
//           respons = 0;
//           break;
//         }
//       }
//
//       // check error also
//       if (strstr((const char *)aDataBuffer, (const char *)"ERROR") != NULL) {
//         respons = 1;
//         break;
//       }
//     }
//
//     yield();
//   }
//
//   // Serial.printf("%d\t%u\t%d\t%s\r\n", respons, aLenOut, id_data,
//   // aDataBuffer);
//   // Serial.println(aDataBuffer);
//
//   // copy it to the out
//   if ((aLenOut != 0) && (aResponOut != NULL) && (aLenOut > id_data)) {
//     // Serial.printf("1 = %s\r\n2= %s\r\n", aDataBuffer, aResponOut);
//
//     memcpy(aResponOut, aDataBuffer, id_data * sizeof(char));
//
//     // Serial.printf("3 = %s\r\n4= %s\r\n", aDataBuffer, aResponOut);
//   }
//
//   // discard other data
//   while (serialSIM800.available() > 0) {
//     serialSIM800.read();
//     yield();
//   }
//
//   // Serial.printf("%d\t%u\t%d\t%s\t%s\r\n", respons, aLenOut, id_data,
//   // aDataBuffer, aResponOut);
//
//   // free the buffer
//   free(aDataBuffer);
//
//   return respons;
// }
