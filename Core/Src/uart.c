#include "uart.h"
#include <stdio.h>

UART_HandleTypeDef *myHuart;

#define rxBufferMax 255

int rxBufferGp;								// get pointer (read)
int rxBufferPp;								// put pointer (write)
uint8_t rxBuffer[rxBufferMax];
uint8_t rxChar;

// init device
void initUart(UART_HandleTypeDef *inHuart)
{
	myHuart = inHuart;
	HAL_UART_Receive_IT(myHuart, &rxChar, 1);
}

// process received charactor
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	rxBuffer[rxBufferPp++] = rxChar;
	rxBufferPp %= rxBufferMax;
	HAL_UART_Receive_IT(myHuart, &rxChar, 1);
}

// get charactor from Buffer
int16_t getChar()
{
	int16_t result;
	if(rxBufferGp == rxBufferPp) return -1;		// 왜 0이라 하지 않고? →  binary 통신 때문에!
	result = rxBuffer[rxBufferGp++];
	rxBufferGp %= rxBufferMax;
	return result;
}

int _write(int file, char*p, int len)
{
	HAL_UART_Transmit(myHuart, (uint8_t *)p, len, 20);
	return len;
}

void transmitPacket(protocol_t data){
	// 사전 준비
	uint8_t txBuffer[] = {STX, 0, 0, 0, 0, ETX};
	txBuffer[1] = data.command;
	txBuffer[2] = (data.data  >> 7) | 0x80;
	txBuffer[3] = (data.data & 0x7f) | 0x80;

	// CRC 계산
	txBuffer[4] = txBuffer[0] + txBuffer[1] + txBuffer[2] + txBuffer[3];

	// 데이터 전송
	HAL_UART_Transmit(myHuart, txBuffer, sizeof(txBuffer), 1);

	// 데이터 전송 완료 대기
	while (HAL_UART_GetState(myHuart) == HAL_UART_STATE_BUSY_TX
			|| HAL_UART_GetState(myHuart) == HAL_UART_STATE_BUSY_TX_RX);

	// ■ UART 상태 장치의 상태를 가져옴. 이후 장치 상태가 만약 전송중이라면 busy_tx 메세지를 출력 송신 중이라면 busy_RX 메세지를 출력
}

// packet 수신
protocol_t receivePacket() {
	protocol_t result;
	uint8_t buffer[6];
	uint8_t count = 0;
	uint32_t timeout;

	int16_t ch = getChar();
	memset(&result, 0, sizeof(buffer));

	if (ch == STX) {
		buffer[count++] = ch;
		timeout = HAL_GetTick();		// ◀ 타임아웃 시작
		//	■ getTick : sysTick의 count가 몇인지 가져옴 → 일정 시간동안 지나면 카운트가 끝나지 않아도 프로그램 조작 가능
		//	■ sysTick : 32-bit count로 1ms마다 check
		while (ch != ETX) {
			ch = getChar();
			if (ch != -1) {		//  데이터를 수신받았다면 이라는 뜻
				buffer[count++] = ch;
			}
			// ▼ 타임 아웃 계산
			if (HAL_GetTick() - timeout >= 2)
				return result;	// timeout이 2ms
		}
		//	▼ CRC 검사
		uint8_t crc = 0;
		for (int i = 0; i < 4; i++)
			crc += buffer[i];
		if (crc != buffer[4])
			return result;

		// 수신완료 후 데이터 파싱(parsing)
		result.command = buffer[1];
		result.data = buffer[3] & 0x7f;
		result.data |= (buffer[2] & 0x7f << 7);
	}
	return result;
}












