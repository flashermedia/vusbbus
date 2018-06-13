//---------------------------------------------------------------------------
extern "C" {
#include <ntddk.h>
}
#include "EncDecSim.h"
//---------------------------------------------------------------------------
#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, InitTransform2)
#pragma alloc_text (PAGE, Transform2)
#pragma alloc_text (PAGE, Encode)
#pragma alloc_text (PAGE, Decode)
#pragma alloc_text (PAGE, GetCode)
#pragma alloc_text (PAGE, HashDWORD)
#endif
//---------------------------------------------------------------------------
// Массив положения тапов в зависимости от битов D2, D1 во входной комбинации
static DWORD factLFSRArray[] = {
    0x480, // 10, 7         10010000000
    0x4A0, // 10, 7, 5      10010100000
    0x580, // 10, 8, 7      10110000000
    0x5A0  // 10, 8, 7, 5   10110100000
};
// Константа, определяющая кол-во битов LFSR
#define LFSR_BITS   11
// Процедура для получения данных из ST
#define GET_FROM_ST(in5Bit, secTable) (((secTable[((in5Bit) >> 2) & 0xFE]) >> (31 - (in5Bit) & 7)) & 1)
//---------------------------------------------------------------------------
extern "C" DWORD Transform2(DWORD in5Bit, KEY_INFO *keyInfo) {
    // Ограничиваем входное значение 5 битами
    in5Bit=in5Bit & 0x1F;

    // Выбираем положение тапов в зависимости от битов D2, D1 в in5Bit
    DWORD factLFSR = factLFSRArray[(in5Bit >> 1) & 3];

    // Выполняем 1 такт работы LFSR
    DWORD newLFSRState; newLFSRState=0;
    for (int pos=0; pos<LFSR_BITS+1; pos++)
        if ((factLFSR >> pos) & 1)
            newLFSRState^=(keyInfo->curLFSRState >> pos);

    // Учитываем операцию XOR между 2,3 элементом LFSR и битом D0 входной комбинации
    keyInfo->curLFSRState^=(in5Bit & 1) << 2;

    // Переходим к новому состоянию LFSR
    BYTE secretTableTransformResult=GET_FROM_ST(in5Bit, keyInfo->secTable) ^ keyInfo->isInvSecTab;
    keyInfo->curLFSRState=(keyInfo->curLFSRState << 1) | ((newLFSRState ^ secretTableTransformResult) & 1);

    // Учитываем таблицу инверсии для цепи обратной связи
    keyInfo->curLFSRState^=(keyInfo->prepNotMask >> in5Bit) & 1;

    // Возвращаем результат - 10 бит сдвигового регистра
    return ((keyInfo->curLFSRState >> 11) ^ secretTableTransformResult) & 1;
}
//---------------------------------------------------------------------------
extern "C" void InitTransform2(KEY_INFO *keyInfo) {
    // Получаем признак инверсии таблицы ST из 6 бита cryptInitVect
    keyInfo->isInvSecTab=(keyInfo->cryptInitVect >> 5) & 1;

    // Получаем инвертированный 0 бит ST
    BYTE firstBitOfSecTable=GET_FROM_ST(0, keyInfo->secTable) ^ 1;

    // Если он не нулевой, то инвертируем columnMask
    BYTE prepColumnMask=firstBitOfSecTable?keyInfo->columnMask:(~keyInfo->columnMask);

    // Разворачиваем cryptInitVect
    DWORD emulData; emulData=0;
    BYTE cryptInitVect=keyInfo->cryptInitVect & 0x1F;
    for (int bitNum=0; bitNum<4; bitNum++) {
        ((BYTE *)&emulData)[0]<<=2;
        ((BYTE *)&emulData)[0]|=(cryptInitVect & 1) | (((cryptInitVect ^ 1) & 1) << 1);
        cryptInitVect>>=1;
    }
    ((BYTE *)&emulData)[2]=((BYTE *)&emulData)[0]^0xFF;
    ((BYTE *)&emulData)[1]=((BYTE *)&emulData)[0];
    ((BYTE *)&emulData)[3]=((BYTE *)&emulData)[2];
    for (int bitNum=0; bitNum<8; bitNum++) {
        ((BYTE *)&emulData)[1]^=(GET_FROM_ST(bitNum+8*1, keyInfo->secTable) ^
                                    cryptInitVect) << bitNum;
        ((BYTE *)&emulData)[3]^=(GET_FROM_ST(bitNum+8*3, keyInfo->secTable) ^
                                    cryptInitVect) << bitNum;
    }

    // Вычисляем таблицу инверсий для цепи обратной связи
    keyInfo->prepNotMask=0;
    DWORD prepNotMask; prepNotMask=0;
    for (int i=31; i>=0; i--) {
        keyInfo->curLFSRState=prepColumnMask << 3;

        // Определяем информацию на 11 шаге, как для обычного LFSR
        BYTE lfsr11Bit;
        for (int step=0; step<12; step++)
            lfsr11Bit=(BYTE)Transform2(i, keyInfo);

        // Сравниваем с развернутой cryptInitVect
        prepNotMask<<=1;
        prepNotMask|=GET_FROM_ST(i, keyInfo->secTable) ^ (i & 1) ^ ((emulData >> i) & 1) ^ lfsr11Bit;
    }

    keyInfo->prepNotMask=prepNotMask;

    // Преобразуем columnMask в начальное содержимое LFSR
    keyInfo->curLFSRState=
        (prepColumnMask << 3) |
        (firstBitOfSecTable << 2) |
        (firstBitOfSecTable << 1) |
        firstBitOfSecTable;
}
//---------------------------------------------------------------------------
extern "C" VOID Transform(DWORD *Data, KEY_INFO *keyInfo) {
    DWORD i, index, bit;

    InitTransform2(keyInfo);

    for( i = 1, index = 0; i <= 39; ++i ) {
        bit = Transform2( ((BYTE *)Data)[index], keyInfo);

        index = (( (*Data) & 0x01) << 1) | bit;

        if( ( (*Data) & 0x01) == bit )
            *Data = (*Data) >> 1;
        else
            *Data = ( (*Data) >> 1) ^ 0x80500062;
    }
}
//---------------------------------------------------------------------------
#define ROL(x,n) ( ((x) << (n)) | ((x) >> (32-(n))) )
//---------------------------------------------------------------------------
extern "C" void Decode(DWORD *bufPtr, DWORD *nextBufPtr, KEY_INFO *keyInfo) {
    DWORD tmp;

    for (char shiftAmount=25; shiftAmount>=0; shiftAmount-=5) {
        DWORD xorMask=bufPtr[0]^0x5B2C004A;

        DWORD tmp=ROL(xorMask, shiftAmount) ^ bufPtr[1];
        bufPtr[1]=bufPtr[0];
        bufPtr[0]=tmp;
    }

    tmp=bufPtr[0];
    Transform(&bufPtr[0], keyInfo);
    if (nextBufPtr)
        nextBufPtr[1]=bufPtr[0];
    bufPtr[0]^=bufPtr[1];
    bufPtr[1]=tmp;

    for (char shiftAmount=10; shiftAmount>=0;  shiftAmount-=2) {
        DWORD xorMask=bufPtr[0]^0x803425C3;

        DWORD tmp=ROL(xorMask, shiftAmount) ^ bufPtr[1];
        bufPtr[1]=bufPtr[0];
        bufPtr[0]=tmp;
    }

    tmp=bufPtr[0];
    Transform(&bufPtr[0], keyInfo);
    if (nextBufPtr)
        nextBufPtr[0]=bufPtr[0];
    bufPtr[0]^=bufPtr[1];
    bufPtr[1]=tmp;
}
//---------------------------------------------------------------------------
extern "C" void Encode(DWORD *bufPtr, DWORD *nextBufPtr, KEY_INFO *keyInfo) {
    DWORD tmp;

    tmp=bufPtr[1];
    Transform(&bufPtr[1], keyInfo);
    if (nextBufPtr)
        nextBufPtr[0]=bufPtr[1];
    bufPtr[1]^=bufPtr[0];
    bufPtr[0]=tmp;

    for (char shiftAmount=0; shiftAmount<=10;  shiftAmount+=2) {
        DWORD xorMask=bufPtr[1]^0x803425C3;

        DWORD tmp=ROL(xorMask, shiftAmount) ^ bufPtr[0];
        bufPtr[0]=bufPtr[1];
        bufPtr[1]=tmp;
    }

    tmp=bufPtr[1];
    Transform(&bufPtr[1], keyInfo);
    if (nextBufPtr)
        nextBufPtr[1]=bufPtr[1];
    bufPtr[1]^=bufPtr[0];
    bufPtr[0]=tmp;

    for (char shiftAmount=0; shiftAmount<=25; shiftAmount+=5) {
        DWORD xorMask=bufPtr[1]^0x5B2C004A;

        DWORD tmp=ROL(xorMask, shiftAmount) ^ bufPtr[0];
        bufPtr[0]=bufPtr[1];
        bufPtr[1]=tmp;
    }
}
//---------------------------------------------------------------------------
extern "C" void GetCode(unsigned short seed, DWORD *bufPtr, BYTE *secTable) {
    for (int i=0; i<8; i++) {
        ((BYTE *)bufPtr)[i] = 0;
        for (int j=0; j<8; j++) {
            seed *= 0x1989;
            seed += 5;
            BYTE secTablePos=(seed >> 9) & 0x3f;
            BYTE secTableData=(secTable[secTablePos >> 3] >> (7 - (secTablePos & 7))) & 1;
            ((BYTE *)bufPtr)[i] |= secTableData << (7 - j);
        }
    }
}
//---------------------------------------------------------------------------
#pragma warning(disable:4731)
#pragma warning(disable:4102)
extern "C" __declspec (naked) void __fastcall HashDWORD(DWORD *Data, BYTE *edStruct) {
    // Sorry, censored
}
#pragma warning(default:4102)
#pragma warning(default:4731)