#include <windows.h>

#include <tchar.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "turnpix.h"

BOOL g_fGrow;
double g_eAngle;
HBITMAP g_hbmOriginal;
HBITMAP g_hbmRotated;
SIZE g_sizRotated;
COLORREF g_clrBack;
LPTSTR g_pszInput;
LPTSTR g_pszOutput;
float g_dpi;
COLORREF g_clrTransparent;

void Init(void)
{
    g_fGrow = TRUE;
    g_eAngle = 0;
    g_hbmOriginal = NULL;
    g_hbmRotated = NULL;
    g_clrBack = CLR_INVALID;
    g_pszInput = NULL;
    g_pszOutput = NULL;
    g_dpi = 0;
    g_clrTransparent = CLR_INVALID;
}

void Help(void)
{
    printf("turn - 画像を回転します。\n");
    printf("使い方: turn -i 入力 -o 出力 [-a 角度] [-g ON/OFF] [-b #RRGGBB] [-t #RRGGBB]\n");
    printf("\n");
    printf("-g OFFはキャンバスを広げないときに指定します。\n");
    printf("-b には背景色を指定します。\n");
    printf("-t にはGIFの透過色を指定します。\n");
}

void Version(void)
{
    printf("turn ver. 1.7.1 by 片山博文MZ\n");
    printf("katayama.hirofumi.mz@gmail.com\n");
}

INT GetImageType(LPCTSTR pszFileName)
{
    LPCTSTR pch;
    pch = _tcsrchr(pszFileName, '.');
    if (pch == NULL)
        return 0;
    if (lstrcmpi(pch, ".bmp") == 0)
        return BMP;
    if (lstrcmpi(pch, ".gif") == 0)
        return GIF;
    if (lstrcmpi(pch, ".jpg") == 0 || lstrcmpi(pch, ".jpe") == 0 ||
        lstrcmpi(pch, ".jpeg") == 0 || lstrcmpi(pch, ".jfif") == 0)
        return JPEG;
    if (lstrcmpi(pch, ".png") == 0)
        return PNG;
    if (lstrcmpi(pch, ".tif") == 0 || lstrcmpi(pch, ".tiff") == 0)
        return TIFF;
    return 0;
}

BOOL Open(LPCTSTR pszFileName)
{
    HBITMAP hbm;
    float dpi;
    INT i;
    
    i = GetImageType(pszFileName);
    switch(i)
    {
    case BMP:
        hbm = LoadBitmapFromFile(pszFileName, &dpi);
        break;

    case GIF:
        dpi = 0.0;
        hbm = LoadGifAsBitmap(pszFileName, &g_clrTransparent);
        break;

    case JPEG:
        hbm = LoadJpegAsBitmap(pszFileName, &dpi);
        break;

    case PNG:
        hbm = LoadPngAsBitmap(pszFileName, &dpi);
        break;

    case TIFF:
        hbm = LoadTiffAsBitmap(pszFileName, &dpi);
        break;
    }

    if (hbm != NULL)
    {
        g_dpi = dpi;
        g_hbmOriginal = hbm;
        return TRUE;
    }
    return FALSE;
}

HBITMAP CreateBackBitmap(void)
{
    HBITMAP hbm;
    HDC hdc;
    BITMAPINFO bi;
    VOID *pvBits;

    ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_sizRotated.cx;
    bi.bmiHeader.biHeight = g_sizRotated.cy;
    bi.bmiHeader.biPlanes = 1;
    if (g_clrBack == CLR_INVALID)
        bi.bmiHeader.biBitCount = 32;
    else
        bi.bmiHeader.biBitCount = 24;
    hdc = CreateCompatibleDC(NULL);
    if (hdc != NULL)
    {
        hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
        if (hbm != NULL && g_clrBack != CLR_INVALID)
        {
            RECT rc;
            HBRUSH hbr = CreateSolidBrush(g_clrBack);
            HGDIOBJ hbmOld = SelectObject(hdc, hbm);
            HGDIOBJ hbrOld = SelectObject(hdc, hbr);
            SetRect(&rc, 0, 0, g_sizRotated.cx, g_sizRotated.cy);
            FillRect(hdc, &rc, hbr);
            SelectObject(hdc, hbrOld);
            SelectObject(hdc, hbmOld);
            DeleteObject(hbr);
        }
        DeleteDC(hdc);
    }
    if (g_clrBack == CLR_INVALID)
    {
        INT x, y;
        LPDWORD pdw = (LPDWORD)pvBits;
        for(y = 0; y < bi.bmiHeader.biHeight; y++)
        {
            for(x = 0; x < bi.bmiHeader.biWidth; x++)
            {
                *pdw++ = ((x >> 3) & 1) ^ ((y >> 3) & 1) ? 0xFFC0C0C0 : 0xFFFFFFFF;
            }
        }
    }
    return hbm;
}

BOOL Save(LPCTSTR pszFileName)
{
    HBITMAP hbm;
    INT i;
    BOOL f;
    COLORREF clr = g_clrBack;

    i = GetImageType(pszFileName);

    if (g_clrBack != CLR_INVALID || i == JPEG)
    {
        if (g_clrBack == CLR_INVALID && i == JPEG)
            g_clrBack = RGB(255, 255, 255);
        hbm = CreateBackBitmap();
        AlphaBlendBitmap(hbm, g_hbmRotated);
    }
    else
        hbm = g_hbmRotated;
    if (i == JPEG)
        g_clrBack = clr;

    f = FALSE;
    switch(i)
    {
    case BMP:
        f = SaveBitmapToFile(pszFileName, hbm, g_dpi);
        break;

    case GIF:
        if (IsDIBOpaque(hbm))
        {
            f = Save32BppBitmapAsGif(pszFileName, hbm, g_clrTransparent);
        }
        else
        {
            if (g_clrTransparent == CLR_INVALID)
                g_clrTransparent = g_clrBack;
            f = Save32BppBitmapAsGif(pszFileName, hbm, g_clrTransparent);
        }
        break;

    case JPEG:
        f = SaveBitmapAsJpeg(pszFileName, hbm, 100, FALSE, g_dpi);
        break;

    case PNG:
        f = SaveBitmapAsPngFile(pszFileName, hbm, g_dpi);
        break;

    case TIFF:
        f = SaveBitmapAsTiff(pszFileName, hbm, g_dpi);
        break;
    }
    if (hbm != g_hbmRotated)
        DeleteObject(hbm);

    return f;
}

int main(int argc, char **argv)
{
    int i;
    BOOL f;
    
    if (argc == 1)
    {
        Help();
        return 0;
    }
    if (argc == 2 && lstrcmpi(argv[1], "--help"))
    {
        Help();
        return 0;
    }
    if (argc == 2 && lstrcmpi(argv[1], "--version"))
    {
        Version();
        return 0;
    }

    Init();
    f = TRUE;
    for(i = 1; i < argc; i++)
    {
        if (lstrcmpi(argv[i], "-a") == 0)
        {
            if (i + 1 < argc)
            {
                g_eAngle = atof(argv[++i]) * M_PI / 180.0;;
            }
            else
            {
                f = FALSE;
            }
        }
        else if (lstrcmpi(argv[i], "-i") == 0)
        {
            if (i + 1 < argc)
            {
                g_pszInput = argv[++i];
            }
            else
            {
                f = FALSE;
            }
        }
        else if (lstrcmpi(argv[i], "-o") == 0)
        {
            if (i + 1 < argc)
            {
                g_pszOutput = argv[++i];
            }
            else
            {
                f = FALSE;
            }
        }
        else if (lstrcmpi(argv[i], "-g") == 0)
        {
            if (i + 1 < argc)
            {
                if (lstrcmpi(argv[++i], "ON") == 0)
                {
                    g_fGrow = TRUE;
                }
                else if (lstrcmpi(argv[++i], "OFF") == 0)
                {
                    g_fGrow = FALSE;
                }
                else
                {
                    f = FALSE;
                }
            }
        }
        else if (lstrcmpi(argv[i], "-b") == 0)
        {
            if (i + 1 < argc && argv[++i][0] == '#')
            {
                DWORD dw = strtoul(argv[i] + 1, NULL, 16);
                g_clrBack = RGB((BYTE)HIWORD(dw), HIBYTE(LOWORD(dw)), (BYTE)dw);
            }
            else
            {
                f = FALSE;
            }
        }
        else if (lstrcmpi(argv[i], "-t") == 0)
        {
            if (i + 1 < argc && argv[++i][0] == '#')
            {
                DWORD dw = strtoul(argv[i] + 1, NULL, 16);
                g_clrTransparent = RGB((BYTE)HIWORD(dw), HIBYTE(LOWORD(dw)), (BYTE)dw);
            }
            else
            {
                f = FALSE;
            }
        }
    }
    if (g_pszInput == NULL || g_pszOutput == NULL || !f)
    {
        printf("引数が間違っています。\n");
        Help();
        return 1;
    }

    if (!Open(g_pszInput))
    {
        printf("%s: ファイルが開けないか、形式が間違っています。\n", g_pszInput);
        return 2;
    }

    g_hbmRotated = CreateRotated32BppBitmap(g_hbmOriginal, g_eAngle, g_fGrow);
    if (g_hbmRotated == NULL)
    {
        printf("メモリが足りません。\n");
        DeleteObject(g_hbmOriginal);
        return 2;
    }
    else
    {
        BITMAP bm;
        GetObject(g_hbmRotated, sizeof(BITMAP), &bm);
        g_sizRotated.cx = bm.bmWidth;
        g_sizRotated.cy = bm.bmHeight;
    }

    if (!Save(g_pszOutput))
    {
        printf("%s: ファイルが出力できませんでした。\n", g_pszOutput);
        DeleteObject(g_hbmOriginal);
        DeleteObject(g_hbmRotated);
        return 3;
    }

    DeleteObject(g_hbmOriginal);
    DeleteObject(g_hbmRotated);

    return 0;
}
