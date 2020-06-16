//
//  ttfparser.hpp
//  ttfparser
//
//  Created by SunDong on 2020/6/16.
//  Copyright © 2020 SunDong. All rights reserved.
//

/**
 从ttf文件解析获取字体文件名
 */
#ifndef ttfparser_h
#define ttfparser_h

#include <string.h>
#include <ctype.h>
#import <Foundation/Foundation.h>

typedef struct _tagTT_TABLE_DIRECTORY{  //16位
    char    szTag[4];           //table name
    unsigned int   uCheckSum;          //Check sum
    unsigned int   uOffset;            //Offset from beginning of file
    unsigned int  uLength;            //length of the table in bytes
}TT_TABLE_DIRECTORY;

typedef struct _tagTT_OFFSET_TABLE{ //12位
    unsigned short  uMajorVersion;
    unsigned short  uMinorVersion;
    unsigned short  uNumOfTables;
    unsigned short  uSearchRange;
    unsigned short  uEntrySelector;
    unsigned short  uRangeShift;
}TT_OFFSET_TABLE;

typedef struct _tagTT_NAME_TABLE_HEADER{
    unsigned short  uFSelector;         //format selector. Always 0
    unsigned short  uNRCount;           //Name Records count
    unsigned short  uStorageOffset;     //Offset for strings storage, from start of the table
}TT_NAME_TABLE_HEADER;


typedef struct _tagTT_NAME_RECORD{
    unsigned short  uPlatformID;
    unsigned short  uEncodingID;
    unsigned short  uLanguageID;
    unsigned short  uNameID;
    unsigned short  uStringLength;
    unsigned short  uStringOffset;  //from start of storage area
}TT_NAME_RECORD;


typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int INT32;

#define LOBYTE(w)           ((BYTE)(((WORD)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((WORD)(w)) >> 8) & 0xff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((WORD)(a)) & 0xff)) | ((WORD)((BYTE)(((WORD)(b)) & 0xff))) << 8))

#define SWAPWORD(x)     MAKEWORD(HIBYTE(x), LOBYTE(x))

#define HIWORD(I) ((WORD) (((INT32)( I ) >> 16) & 0xFFFF))
#define LOWORD(l) ((WORD)(((INT32)(l)) & 0xffff))
#define MAKEINT32(a, b) ((INT32)(((WORD)(a)) | ((INT32)((WORD)(b))) << 16))
#define SWAPINT32(x)     MAKEINT32(SWAPWORD(HIWORD(x)), SWAPWORD(LOWORD(x)))

#define    MAJOR_TTF_OUTLINES    1
#define    MINOR_TTF_OUTLINES    0

// OTTO
#define    MAJOR_CFF_OUTLINES   0x4f54
#define    MINOR_CFF_OUTLINES   0x544f

NSMutableDictionary* GetFontNameFromFile(char* ttfFilepath)
{
    NSMutableDictionary* fontNameDic = [NSMutableDictionary dictionary];
    FILE *fp;
    fp = fopen(ttfFilepath,"r");
    if(fp)
    {
        TT_OFFSET_TABLE ttOffsetTable;
    
        fread(&ttOffsetTable, sizeof(TT_OFFSET_TABLE), 1, fp);
        
        ttOffsetTable.uNumOfTables = SWAPWORD(ttOffsetTable.uNumOfTables);
        ttOffsetTable.uMajorVersion = SWAPWORD(ttOffsetTable.uMajorVersion);
        ttOffsetTable.uMinorVersion = SWAPWORD(ttOffsetTable.uMinorVersion);
        bool isvalidVersion = false;
        //check is this is a true type font and the version is 1.0
        if((ttOffsetTable.uMajorVersion == MAJOR_TTF_OUTLINES)
           && (ttOffsetTable.uMinorVersion == MINOR_TTF_OUTLINES))
        {
            isvalidVersion = true;
        }else if((ttOffsetTable.uMajorVersion == MAJOR_CFF_OUTLINES)
                 && (ttOffsetTable.uMinorVersion == MINOR_CFF_OUTLINES))
        {
            isvalidVersion = true;
        }
        if(!isvalidVersion)
            return nil;

        TT_TABLE_DIRECTORY tblDir;
        bool bFound = false;
        for(int i = 0; i < ttOffsetTable.uNumOfTables; i++)
        {
            fread(&tblDir, sizeof(TT_TABLE_DIRECTORY), 1, fp);
            if (tblDir.szTag[0] == 'n' && tblDir.szTag[1] == 'a' && tblDir.szTag[2] == 'm' && tblDir.szTag[3] == 'e')
            {
                bFound = true;
                tblDir.uLength = SWAPINT32(tblDir.uLength);
                tblDir.uOffset = SWAPINT32(tblDir.uOffset);
                break;
            }
        }
        
        if(bFound)
        {
            fseek(fp,tblDir.uOffset,SEEK_SET); //文件指针移动到从开始到tblDir.uOffset的距离
            TT_NAME_TABLE_HEADER ttNTHeader;
            fread(&ttNTHeader, sizeof(TT_NAME_TABLE_HEADER), 1, fp);
            ttNTHeader.uNRCount = SWAPWORD(ttNTHeader.uNRCount);
            ttNTHeader.uStorageOffset = SWAPWORD(ttNTHeader.uStorageOffset);
            TT_NAME_RECORD ttRecord = {0};
            bFound = false;

            for(int i=0; i<ttNTHeader.uNRCount; i++)
            {
                fseek(fp, tblDir.uOffset + sizeof(TT_NAME_TABLE_HEADER) + i*sizeof(TT_NAME_RECORD), SEEK_SET);
                fread(&ttRecord, sizeof(TT_NAME_RECORD), 1, fp);
                
                ttRecord.uNameID = SWAPWORD(ttRecord.uNameID);
                ttRecord.uPlatformID = SWAPWORD(ttRecord.uPlatformID); //3：微软
                ttRecord.uEncodingID = SWAPWORD(ttRecord.uEncodingID); // 3: PRC编码 4字节代表一个汉字
                ttRecord.uLanguageID = SWAPWORD(ttRecord.uLanguageID);
                ttRecord.uStringLength = SWAPWORD(ttRecord.uStringLength);
                ttRecord.uStringOffset = SWAPWORD(ttRecord.uStringOffset);
                
                if((ttRecord.uLanguageID&0x03ff) != 0x09 && ttRecord.uLanguageID != 2052) //英文或中文
                    continue;
                if(ttRecord.uNameID != 1)
                    continue;
                fseek(fp, tblDir.uOffset + ttNTHeader.uStorageOffset + ttRecord.uStringOffset, SEEK_SET);
                
                //bug fix: see the post by SimonSays to read more about it
                char *lpszNameBuf = (char*)malloc(ttRecord.uStringLength + 1);
                memset(lpszNameBuf, 0, ttRecord.uStringLength + 1);
                fread(lpszNameBuf, ttRecord.uStringLength, 1, fp);
                
                NSData *data = [NSData dataWithBytes:lpszNameBuf length:(ttRecord.uStringLength + 1)];
                NSString* nsFontName= [[NSString alloc]initWithData:data  encoding:NSUTF16StringEncoding];
                free(lpszNameBuf);
                if(nsFontName.length > 0)
                {
                    if(ttRecord.uLanguageID != 2052) //中文字体名
                    {
                        [fontNameDic setObject:nsFontName forKey:@"CN"];  //此处为windowS下打开字体文件的字体名称，不一定是中文名称
                    }
                    else if((ttRecord.uLanguageID&0x03ff) != 0x09) //英文字符名,有可能不存在
                    {
                        [fontNameDic setObject:nsFontName forKey:@"EN"];
                    }
                }
            }
                
        }
        fclose(fp);
    }
    return fontNameDic;
}

#endif /* ttfparser_h */
