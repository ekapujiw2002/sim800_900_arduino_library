/*
 * sim800.cpp
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

#include "sim800.h"

void SIM800::preInit(void) {
  if (SIM800_POWER_STATUS != -1) {
    pinMode(SIM800_POWER_STATUS, INPUT);
    delay(10);
    if (LOW == digitalRead(SIM800_POWER_STATUS)) {
      if (sendATTest() != 0) {
        delay(800);
        digitalWrite(SIM800_POWER_PIN, HIGH);
        delay(200);
        digitalWrite(SIM800_POWER_PIN, LOW);
        delay(2000);
        digitalWrite(SIM800_POWER_PIN, HIGH);
        delay(3000);
      }
      while (sendATTest() != 0) {
        delay(500);
        yield();
      }
      // Serial.println("Init O.K!");
    } else {
      ERROR("Power check failed!");
    }
  } else {
    if (sendATTest() != 0) {
      delay(800);
      digitalWrite(SIM800_POWER_PIN, HIGH);
      delay(200);
      digitalWrite(SIM800_POWER_PIN, LOW);
      delay(2000);
      digitalWrite(SIM800_POWER_PIN, HIGH);
      delay(3000);
    }
    while (sendATTest() != 0) {
      delay(500);
      yield();
    }
    // Serial.println("Init O.K!");
  }
}

int SIM800::checkReadable(void) { return serialSIM800.available(); }

int SIM800::readBuffer(char *buffer, int count, unsigned int timeOut) {
  int i = 0;
  unsigned long timerStart, timerEnd;
  timerStart = millis();
  while (1) {
    while (serialSIM800.available()) {
      char c = serialSIM800.read();
      if (c == '\r' || c == '\n')
        c = '$';
      buffer[i++] = c;
      if (i > count - 1)
        break;

      yield();
      delay(1);
    }
    if (i > count - 1)
      break;
    timerEnd = millis();
    if (timerEnd - timerStart > 1000 * timeOut) {
      break;
    }

    yield();
    delay(1);
  }
  delay(500);
  while (serialSIM800.available()) { // display the other thing..
    serialSIM800.read();
    yield();
    delay(1);
  }
  return 0;
}

void SIM800::cleanBuffer(char *buffer, int count) {
  for (int i = 0; i < count; i++) {
    buffer[i] = '\0';
  }
}

void SIM800::sendCmd(const char *cmd) { serialSIM800.write(cmd); }

int SIM800::sendATTest(void) {
  int ret = sendCmdAndWaitForResp("AT\r\n", "OK", DEFAULT_TIMEOUT);
  return ret;
}

int SIM800::waitForResp(const char *resp, unsigned int timeout) {
  int len = strlen(resp);
  int sum = 0;
  unsigned long timerStart, timerEnd;
  timerStart = millis();

  while (1) {
    if (serialSIM800.available()) {
      char c = serialSIM800.read();
      sum = (c == resp[sum]) ? sum + 1 : 0;
      if (sum == len)
        break;
    }
    timerEnd = millis();
    if (timerEnd - timerStart > 1000 * timeout) {
      return -1;
    }

    yield();
    delay(1);
  }

  while (serialSIM800.available()) {
    serialSIM800.read();
    yield();
    delay(1);
  }

  return 0;
}

void SIM800::sendEndMark(void) { serialSIM800.println((char)26); }

int SIM800::sendCmdAndWaitForResp(const char *cmd, const char *resp,
                                  unsigned timeout) {
  sendCmd(cmd);
  return waitForResp(resp, timeout);
}

void SIM800::serialDebug(void) {
  while (1) {
    if (serialSIM800.available()) {
      Serial.write(serialSIM800.read());
    }
    if (Serial.available()) {
      serialSIM800.write(Serial.read());
    }

    yield();
    delay(1);
  }
}

/**
 * reset modem using the pin reset
 * @method SIM800::modemReset
 */
int SIM800::modemReset() {
  // delay(1000);
  digitalWrite(SIM800_POWER_PIN, HIGH);
  delay(500);
  digitalWrite(SIM800_POWER_PIN, LOW);
  delay(2000);
  digitalWrite(SIM800_POWER_PIN, HIGH);
  delay(3000);

  return sendCmdAndWaitForResp("AT\r\nAT\r\n", "SMS Ready", 30000, 0, NULL);
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
int SIM800::sendCmdAndWaitForResp(const char *aCmd, const char *aResponExit,
                                  const uint32_t aTimeoutMax,
                                  const char aLenOut, char *aResponOut) {

#define MAX_BUFFER_TMP 128

  int id_data, respons = 0;
  uint32_t uart_tout_cnt = 0;

  char *aDataBuffer = (char *)calloc(MAX_BUFFER_TMP, sizeof(char));

  // buffer created???
  if (aDataBuffer == NULL) {
    return -1;
  }

  // reset to all 0
  memset(aDataBuffer, '\0', MAX_BUFFER_TMP);

  // read left buffer data
  // discard all received data, if any
  while (serialSIM800.available() > 0) {
    serialSIM800.read();
    yield();
  }

  // send command
  if (aCmd != NULL) {
    DEBUG(String("DEBUG : TX   => ") + String(aCmd));
    serialSIM800.write(aCmd);
  }

  // wait for reply
  id_data = 0;
  uart_tout_cnt = 0;
  while ((id_data < (MAX_BUFFER_TMP - 1)) && (uart_tout_cnt <= aTimeoutMax)) {
    // get uart data or timeout
    uart_tout_cnt = 0;
    while ((serialSIM800.available() == 0) && (uart_tout_cnt < aTimeoutMax))
    // wait data arrive or tout
    {
      uart_tout_cnt++;
      yield();
      delay(1);
    }

    // check for timeout
    if (uart_tout_cnt >= aTimeoutMax) {
      respons = -2;
      break;
    } else {
      aDataBuffer[id_data] = (char)serialSIM800.read();
      id_data++;

      // check if the desired answer  is in the response of the module
      if (aResponExit != NULL) {
        if (strstr((const char *)aDataBuffer, (const char *)aResponExit) !=
            NULL) {
          respons = 0;
          break;
        }
      }

      // check error also
      if (strstr((const char *)aDataBuffer, (const char *)"ERROR") != NULL) {
        respons = 1;
        break;
      }
    }

    yield();
  }

  // copy it to the out
  if ((aLenOut != 0) && (aResponOut != NULL) && (aLenOut > id_data)) {
    memcpy(aResponOut, aDataBuffer, id_data * sizeof(char));
  }

  // discard other data
  while (serialSIM800.available() > 0) {
    serialSIM800.read();
    yield();
  }

  // free the buffer
  free(aDataBuffer);

  DEBUG(String("DEBUG : RX   => ") + respons);
  DEBUG(String("DEBUG : DATA => ") + String(aResponOut));

  return respons;
}
