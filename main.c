/*
 * LED_matrix.c
 *
 * Created: 2018-02-13 21:39:11
 * Author : Nikodem Kastelik
 */ 

#define F_CPU 1000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define PASTER(x,y) x##y
#define GLUE(x,y) PASTER(x,y)
#define IS(x,y) (x == y ? 1 : 0)
#define NOT(x) (!(x))
#define IS_BIT_SET(x,y) (x & y ? 1 : 0)

//#define PROGMEM_MSG(x) ((uint8_t*)pgm_read_word(&log_msg[x]))

#define LEDMATRIX_NO_OF_MOVES 8
#define LEDMATRIX_ROWS_AND_COLS 2
#define LEDMATRIX_NO_OF_ROWS 8
#define LEDMATRIX_NO_OF_COLS 8
#define LEDMATRIX_FIRST_ROW 0
#define LEDMATRIX_LAST_ROW (LEDMATRIX_NO_OF_ROWS-1)
#define LEDMATRIX_FIRST_COL 0
#define LEDMATRIX_LAST_COL (LEDMATRIX_NO_OF_COLS-1)
#define LEDMATRIX_BUFFER_BETWEEN_PRINTS 1
#define LEDMATRIX_SIZE	8

#define ROW 0
#define COL 1
#define ROWPORT		D
#define COLPORT		B
#define ROW_RESET_VALUE 0x00
#define COL_RESET_VALUE 0xFF
#define DELAY_BETWEEN_MOVES_US 50
#define NEXT_DIRECTION 1

#define ANIMATION_INITIALIZER LEDMATRIX_BUFFER_BETWEEN_PRINTS

#define GET_MOVE_INDEX(x) (x / (LEDMATRIX_SIZE + LEDMATRIX_BUFFER_BETWEEN_PRINTS))
#define GET_SHIFT_INDEX(x) (x % (LEDMATRIX_SIZE + LEDMATRIX_BUFFER_BETWEEN_PRINTS))
#define GET_ELEMENT_INDEX(x) ((GET_SHIFT_INDEX(x)) - LEDMATRIX_BUFFER_BETWEEN_PRINTS)

#define FORWARD		0
#define BACKWARD	1
#define GET_SHIFT_DIRECTION(x) (x % 2)


#define CARRY_DISABLED	1<<8
#define CARRY_DO_ROLL	1<<9
#define CARRY_USE_PROVIDED 1<<10
#define GET_PROVIDED_CARRY(x) (x & 0x00ff)

typedef enum direction						{UP,					DOWN,					LEFT,					RIGHT} direction_t;
const int8_t VECTOR_TO_SAVE_AS_CARRY[] =	{LEDMATRIX_FIRST_ROW,	LEDMATRIX_LAST_ROW,		LEDMATRIX_FIRST_COL,	LEDMATRIX_LAST_COL};
const int8_t VECTOR_TO_LOAD_CARRY_INTO[] =	{LEDMATRIX_LAST_ROW,	LEDMATRIX_FIRST_ROW,	LEDMATRIX_LAST_COL,		LEDMATRIX_FIRST_COL};
	
typedef enum interruptStatus {NOT_OCCURED, OCCURED} interrupt_status_t;
volatile interrupt_status_t interrupt_status = NOT_OCCURED;

uint8_t letterK[LEDMATRIX_NO_OF_ROWS] = {0x63, 0x73, 0x3f, 0x0f, 0x1f, 0x3b, 0x73, 0x63};
uint8_t letterO[LEDMATRIX_NO_OF_ROWS] = {0x3c, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x3c};
uint8_t letterC[LEDMATRIX_NO_OF_ROWS] = {0x3c, 0x7e, 0x66, 0x06, 0x06, 0x66, 0x7e, 0x3c};
uint8_t letterH[LEDMATRIX_NO_OF_ROWS] = {0x66, 0x66, 0x66, 0x7e, 0x7e, 0x66, 0x66, 0x66};
uint8_t letterA[LEDMATRIX_NO_OF_ROWS] = {0x18, 0x3c, 0x66, 0x66, 0x66, 0x7e, 0x7e, 0x66};
uint8_t letterM[LEDMATRIX_NO_OF_ROWS] = {0x42, 0x66, 0x7e, 0x5a, 0x42, 0x42, 0x42, 0x42};
uint8_t letterI[LEDMATRIX_NO_OF_ROWS] = {0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e};
uint8_t letterE[LEDMATRIX_NO_OF_ROWS] = {0x7e, 0x06, 0x06, 0x3e, 0x3e, 0x06, 0x06, 0x7e};
	
uint8_t ledHeartPattern[LEDMATRIX_NO_OF_ROWS] = {0x00, 0x66, 0xff, 0xff, 0xff, 0x7e, 0x3c, 0x18};
uint8_t emptyPattern[LEDMATRIX_NO_OF_ROWS] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	
volatile uint8_t currentPattern[LEDMATRIX_NO_OF_ROWS] = {};
	

void RowToColumnMatrix(uint8_t * inputMatrix, uint8_t * outputMatrix)
{
	uint8_t thisColumn;
	for(uint8_t col=0; col < LEDMATRIX_NO_OF_COLS; col++)
	{
		thisColumn = 0x00;
		for(uint8_t row=0; row < LEDMATRIX_NO_OF_ROWS; row++)
		{
			thisColumn |= (inputMatrix[row] & (1<<col) ? 1 : 0) << row;
		}
		outputMatrix[col] = thisColumn;
	}
}

void ColumnToRowMatrix(uint8_t * inputMatrix, uint8_t * outputMatrix)
{
	uint8_t thisRow;
	for(uint8_t row=0; row < LEDMATRIX_NO_OF_ROWS; row++)
	{
		thisRow = 0x00;
		for(uint8_t col=0; col < LEDMATRIX_NO_OF_COLS; col++)
		{
			thisRow |= (inputMatrix[col] & (1<<row) ? 1 : 0) << col;
		}
		outputMatrix[row] = thisRow;
	}	
}

uint8_t getColumn(uint8_t * inputMatrix, uint8_t desiredColumnIndex)
{
	uint8_t desiredColumn = 0x00;
	for(uint8_t row=0; row < LEDMATRIX_NO_OF_ROWS; row++)
	{
		desiredColumn |= (inputMatrix[row] & (1<<desiredColumnIndex) ? 1 : 0) << row;
	}
	return desiredColumn;
}

uint8_t getRow(uint8_t * inputMatrix, uint8_t desiredRowIndex)
{
	return inputMatrix[desiredRowIndex];
}

void shiftPattern(uint8_t * pattern, uint8_t shiftDirection)
{
	if(shiftDirection == FORWARD)
	{
		for(uint8_t vectorIndex=0 ; vectorIndex < LEDMATRIX_SIZE-1  ; vectorIndex++)
		{
			pattern[vectorIndex] = pattern[vectorIndex+1];
		}
	}
	else if(shiftDirection == BACKWARD)
	{
		for(uint8_t vectorIndex=LEDMATRIX_SIZE-1 ; vectorIndex > 0 ; vectorIndex--)
		{
			pattern[vectorIndex] = pattern[vectorIndex-1];
		}
	}
}

void shiftPatternWithCarry(direction_t thisDirection, uint8_t * pattern, int16_t carryFlag)
{	
	uint8_t carry = 0x00;
	uint8_t * patternToProcess = pattern;
	if(IS(thisDirection , LEFT) || IS(thisDirection , RIGHT))
	{
		uint8_t columnMatrix[LEDMATRIX_NO_OF_COLS];
		RowToColumnMatrix(pattern, columnMatrix);
		patternToProcess = columnMatrix;
	}
	
	if(IS_BIT_SET(carryFlag , CARRY_DO_ROLL))
	{
		carry = patternToProcess[VECTOR_TO_SAVE_AS_CARRY[thisDirection]];
	}
	else if(IS_BIT_SET(carryFlag , CARRY_USE_PROVIDED))
	{
		carry = GET_PROVIDED_CARRY(carryFlag);
	}
	else if(IS_BIT_SET(carryFlag , CARRY_DISABLED))
	{
		carry = 0x00;
	}
	
	shiftPattern(patternToProcess, GET_SHIFT_DIRECTION(thisDirection));
	
	patternToProcess[VECTOR_TO_LOAD_CARRY_INTO[thisDirection]] = carry;
	
	if(IS(thisDirection , LEFT) || IS(thisDirection , RIGHT))
	{
		ColumnToRowMatrix(patternToProcess, pattern);
	}
	
}

void rollPattern(direction_t thisDirection, uint8_t * sourcePattern, uint8_t * destinationPattern, uint16_t moveIndex)
{
	int8_t thisShiftIndex = GET_SHIFT_INDEX(moveIndex);
	
	if(thisShiftIndex < LEDMATRIX_BUFFER_BETWEEN_PRINTS)
	{
		shiftPatternWithCarry(thisDirection, destinationPattern, CARRY_DISABLED);
	}
	else
	{
		uint8_t nextPatternElement = 0x00;
		if(IS(thisDirection , UP) || IS(thisDirection , DOWN))
		{
			nextPatternElement = getRow(sourcePattern, GET_ELEMENT_INDEX(moveIndex));
		}
		else if(IS(thisDirection , LEFT) || IS(thisDirection , RIGHT))
		{
			nextPatternElement = getColumn(sourcePattern, GET_ELEMENT_INDEX(moveIndex));
		}
		shiftPatternWithCarry(thisDirection, destinationPattern, (uint16_t)(nextPatternElement | CARRY_USE_PROVIDED));
	}
}

void loadPattern(uint8_t * inputPattern, uint8_t * outputPattern)
{
	for(uint8_t row=0 ; row < LEDMATRIX_NO_OF_ROWS ; row++)
	{
		outputPattern[row] = inputPattern[row];
	}
}

void displayPattern(uint8_t * patternToDisplay)
{
	for(uint8_t row=0 ; row < LEDMATRIX_NO_OF_ROWS ; row++)
	{
		if(IS(interrupt_status, OCCURED))
		{
			interrupt_status = NOT_OCCURED;
			break;
		}
		GLUE(PORT,ROWPORT) = (1<<row);	
		for(uint8_t col=0 ; col < LEDMATRIX_NO_OF_COLS ; col++)
		{
			uint8_t mask = ~(currentPattern[row] & (1<<col));
			GLUE(PORT,COLPORT) = mask;
			//if(NOT(IS(mask, COL_RESET_VALUE)))
			//{
			//	_delay_us(DELAY_BETWEEN_MOVES_US);
			//}
		}
	}
}

ISR(TIMER0_OVF_vect)
{
	static int8_t tick;
	if(IS(tick , 1))
	{
		tick = 0;
		static int16_t counter;
		static direction_t mode = LEFT;
		interrupt_status = OCCURED;
		GLUE(PORT,ROWPORT) = 0x00;
		uint8_t thisMove = GET_MOVE_INDEX(counter);
		switch(thisMove)
		{
			case(0): rollPattern(mode, ledHeartPattern, (uint8_t*)currentPattern, counter); break;
			case(1): rollPattern(mode, letterK, (uint8_t*)currentPattern, counter); break;
			case(2): rollPattern(mode, letterO, (uint8_t*)currentPattern, counter); break;
			case(3): rollPattern(mode, letterC, (uint8_t*)currentPattern, counter); break;
			case(4): rollPattern(mode, letterH, (uint8_t*)currentPattern, counter); break;
			case(5): rollPattern(mode, letterA, (uint8_t*)currentPattern, counter); break;
			case(6): rollPattern(mode, letterM, (uint8_t*)currentPattern, counter); break;
			case(7): rollPattern(mode, ledHeartPattern, (uint8_t*)currentPattern, counter); break;
			case(8): rollPattern(mode, letterC, (uint8_t*)currentPattern, counter); break;
			case(9): rollPattern(mode, letterI, (uint8_t*)currentPattern, counter); break;
			case(10): rollPattern(mode, letterE, (uint8_t*)currentPattern, counter); break;
			case(11): rollPattern(mode, emptyPattern, (uint8_t*)currentPattern, counter); break;
			case(12):
			{
				 mode = (mode+1) % 4; 
				 counter = -1;
				 break;
			}
		}
		counter++;
	}
	else
	{
		tick++;
	}
}

void initializeGpio(void)
{
	GLUE(PORT,ROWPORT) = ROW_RESET_VALUE;
	GLUE(DDR, ROWPORT) = 0xFF;
	
	GLUE(PORT,COLPORT) = COL_RESET_VALUE;
	GLUE(DDR, COLPORT) = 0xFF;
}

void initializeTimer0(void)
{
	TCCR0 = (1<<CS00); //| (1<<CS00); // 256 prescaler 
	TIMSK = (1<<TOIE0); 
}


int main(void)
{
	initializeGpio();
	initializeTimer0();
	sei();
	
	loadPattern(emptyPattern, (uint8_t*)currentPattern);
    while (1) 
    {
		displayPattern((uint8_t*)currentPattern);
    }
}

