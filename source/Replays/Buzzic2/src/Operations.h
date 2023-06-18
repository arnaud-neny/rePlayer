#pragma once
#include <Core.h>

// Operators
enum {	OP_PUSH_STREAM, OP_PUSH_CONST, OP_PUSH_NOTE, OP_PUSH_TIME,
		OP_SIN, OP_SAW, OP_NOISE, OP_EXP, OP_COMPRESS,
		OP_MUL, OP_ADD,
		OP_ADSR,										// Parameters: attack, decay, sustain relative time, relative sustain level
		OP_LP_FILTER, OP_HP_FILTER, OP_BP_FILTER,		// Push frequency*2*PI then quality (0..1)
		OP_PAN,											// Push % of sound on right channel
		OP_ECHO,										// Parameters: damping, delay, repeats. If delay is negative, then echo panning is performed
		OP_PUSH_CURRENT,
		OP_SIN_G, OP_SAW_G
};

// Parameter types
enum { PT_BYTE, PT_FLOAT, PT_STREAM, PT_FLT_TYPE };

// Single operation.
// !!! NEVER add members to child classes since constant operator size is used in file loading !!!
struct Operation
{
	uint32_t _vtbl;	// removed the virtual function and add this placeholder fake vtbl entry to match x86
	int type;
	char name[128];
	int32_t size[2];
	int32_t pos[2];
	int op;
	int inputCount;
	int paramCount;
	float params[16];
	int paramTypes[16];
	char paramNames[16][16];
	uint32_t backColor;
	// Constructor
	Operation ( )
	{
		type = -1;
		inputCount = 0;
		paramCount = 0;
		size[0] = 50; size[1] = 15;
		memset(name, 0, sizeof(name));
// 		backColor = RGB(255,240,255);
		memset( params, 0, sizeof(params));
		memset( paramTypes, 0, sizeof(paramTypes));
		memset( paramNames, 0, sizeof(paramNames));
	}
	// Returns input position
// 	POINT GetInputPos ( int i )
// 	{
// 		POINT pt = {pos.x + ((i+0.5) - inputCount*0.5) * (2.0*size.cx/inputCount),pos.y - size.cy};
// 		return pt;
// 	}
	// Render operation
// 	void Draw ( HDC hdc, bool focused, System::String* byteSequenceNames[], POINT& offs )
// 	{
// 		RECT r = GetRect ( );
// 		HBRUSH hbr = CreateSolidBrush ( backColor );
// 		HBRUSH hOldBrush = (HBRUSH)SelectObject ( hdc, hbr );
// 		::Rectangle ( hdc, r.left + offs.x, r.top + offs.y, r.right + offs.x, r.bottom + offs.y );
// 		if ( focused )
// 			::Rectangle ( hdc, r.left+2 + offs.x, r.top+2 + offs.y, r.right-2 + offs.x, r.bottom-2 + offs.y);
// 		SetTextAlign ( hdc, TA_CENTER );
// 		COLORREF bk = SetBkColor ( hdc, backColor );
// 		TextOut ( hdc, pos.x + offs.x, r.top + 4 + offs.y, name, strlen(name) );
// 		SetBkColor ( hdc, bk );
// 		SelectObject ( hdc, hOldBrush );
// 		DeleteObject ( hbr );
// 		// Output
// 		Ellipse ( hdc, pos.x - 4 + offs.x, r.bottom - 2 + offs.y, pos.x + 4 + offs.x, r.bottom + 6 + offs.y );
// 		// Input
// 		for ( int i = 0; i < inputCount; i++ ) {
// 			POINT ptI = GetInputPos ( i );
// 			Ellipse ( hdc, ptI.x - 4 + offs.x, ptI.y + 2 + offs.y, ptI.x + 4 + offs.x, ptI.y - 6 + offs.y );
// 		}
// 		// Parameters
// 		SetTextAlign ( hdc, TA_LEFT );
// 		for ( int i = 0; i < paramCount; i++ ) {
// 			RECT r1 = GetParamRect ( i );
// 			::Rectangle ( hdc, r1.left + offs.x, r1.top + offs.y, r1.right + offs.x, r1.bottom + offs.y );
// 			if ( paramNames[i][0] ) {
// 				bk = SetBkColor ( hdc, backColor );
// 				TextOut ( hdc, r.left + 4 + offs.x, r1.top + 1 + offs.y, paramNames[i], strlen(paramNames[i]) );
// 				SetBkColor ( hdc, bk );
// 			}
// 			char buf[128] = {0};
// 			if ( paramTypes[i] == PT_STREAM ) {
// 				int k = (int)params[i];
// 				if ( k >= 0 )
// 					sprintf ( buf, "%d", k );
// 				else if ( byteSequenceNames[-k-1] )
// 						strcpy ( buf, StringToChar ( byteSequenceNames[-k-1] ) );
// 			}
// 			// Filter element is a special
// 			if ( paramTypes[i] == PT_FLT_TYPE ) {
// 				if ( type == OP_LP_FILTER )
// 					strcpy ( buf, "Low pass" );
// 				if ( type == OP_HP_FILTER )
// 					strcpy ( buf, "High pass" );
// 				if ( type == OP_BP_FILTER )
// 					strcpy ( buf, "Band pass" );
// 			}
// 			if ( paramTypes[i] == PT_BYTE )
// 				sprintf ( buf, "%d", (int)params[i] );
// 			if ( paramTypes[i] == PT_FLOAT )
// 				sprintf ( buf, "%.3f", (double)params[i] );
// 			TextOut ( hdc, r1.left + 2 + offs.x, r1.top + 1 + offs.y, buf, strlen(buf) );
// 		}
// 	}
	// Returns bounding rectangle for parameter
// 	RECT GetParamRect ( int p )
// 	{
// 		RECT r = { pos.x - size.cx + 30, pos.y - size.cy + 20 + p*20, pos.x + size.cx-4, pos.y - size.cy + 20 + (p+1)*20 };
// 		return r;
// 	}
	// Returns bounding rectangle
// 	RECT GetRect ( )
// 	{
// 		RECT r = {pos.x - size.cx, pos.y - size.cy, pos.x + size.cx, pos.y + size.cy};
// 		return r;
// 	}
};
struct OperConnection
{
	int operFrom;
	int operTo;
	int inputNo;
};
