#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct tagBITMAPINFOEX
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} BITMAPINFOEX, FAR * LPBITMAPINFOEX;

HBITMAP CreateRotated32BppBitmap(HBITMAP hbmSrc, double angle, BOOL fGrow)
{
    HDC hdc;
    HBITMAP hbm;
    BITMAP bm;
    BITMAPINFO bi;
    BYTE *pbBits, *pbBitsSrc;
    LONG widthbytes, widthbytesSrc;
    INT cost, sint;
    INT cx, cy, x0, x1, y0, y1, px, py, qx, qy;
    BYTE r0, g0, b0, a0, r1, g1, b1, a1;
    INT mx, my;
    INT x, y, ex0, ey0, ex1, ey1;

    if (!GetObject(hbmSrc, sizeof(BITMAP), &bm))
        return NULL;

    if (fGrow)
    {
        cx = (INT)(fabs(bm.bmWidth * cos(angle)) + fabs(bm.bmHeight * sin(angle)) + 0.5);
        cy = (INT)(fabs(bm.bmWidth * sin(angle)) + fabs(bm.bmHeight * cos(angle)) + 0.5);
    }
    else
    {
        cx = bm.bmWidth;
        cy = bm.bmHeight;
    }

    ZeroMemory(&bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = cx;
    bi.bmiHeader.biHeight   = cy;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;

    widthbytesSrc = bm.bmWidth * 4;
    widthbytes = cx * 4;

    hdc = CreateCompatibleDC(NULL);
    if (hdc != NULL)
    {
        hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (VOID **)&pbBits,
                               NULL, 0);
        if (hbm != NULL)
        {
            pbBitsSrc = (BYTE *)HeapAlloc(GetProcessHeap(), 0, widthbytesSrc * bm.bmHeight);
            if (pbBitsSrc != NULL)
                goto success;
            DeleteObject(hbm);
        }
        DeleteDC(hdc);
    }
    return NULL;

success:
    px = (bm.bmWidth - 1) << 15;
    py = (bm.bmHeight - 1) << 15;
    qx = (cx - 1) << 7;
    qy = (cy - 1) << 7;
    cost = cos(angle) * 256;
    sint = sin(angle) * 256;
    bi.bmiHeader.biWidth    = bm.bmWidth;
    bi.bmiHeader.biHeight   = bm.bmHeight;
    GetDIBits(hdc, hbmSrc, 0, bm.bmHeight, pbBitsSrc, &bi, DIB_RGB_COLORS);
    if (bm.bmBitsPixel < 32)
    {
        register UINT cdw = bm.bmWidth * bm.bmHeight;
        register BYTE *pb = pbBitsSrc;
        while(cdw--)
        {
            pb++;
            pb++;
            pb++;
            *pb++ = 0xFF;
        }
    }
    ZeroMemory(pbBits, widthbytes * cy);

    x = (0 - qx) * cost + (0 - qy) * sint + px;
    y = -(0 - qx) * sint + (0 - qy) * cost + py;
    for(my = 0; my < cy; my++)
    {
        /* x = (0 - qx) * cost + ((my << 8) - qy) * sint + px; */
        /* y = -(0 - qx) * sint + ((my << 8) - qy) * cost + py; */
        for(mx = 0; mx < cx; mx++)
        {
            /* x = ((mx << 8) - qx) * cost + ((my << 8) - qy) * sint + px; */
            /* y = -((mx << 8) - qx) * sint + ((my << 8) - qy) * cost + py; */
            x0 = x >> 16;
            x1 = min(x0 + 1, (INT)bm.bmWidth - 1);
            ex1 = x & 0xFFFF;
            ex0 = 0x10000 - ex1;
            y0 = y >> 16;
            y1 = min(y0 + 1, (INT)bm.bmHeight - 1);
            ey1 = y & 0xFFFF;
            ey0 = 0x10000 - ey1;
            if (0 <= x0 && x0 < bm.bmWidth && 0 <= y0 && y0 < bm.bmHeight)
            {
                DWORD c00 = *(DWORD *)&pbBitsSrc[x0 * 4 + y0 * widthbytesSrc];
                DWORD c01 = *(DWORD *)&pbBitsSrc[x0 * 4 + y1 * widthbytesSrc];
                DWORD c10 = *(DWORD *)&pbBitsSrc[x1 * 4 + y0 * widthbytesSrc];
                DWORD c11 = *(DWORD *)&pbBitsSrc[x1 * 4 + y1 * widthbytesSrc];
                b0 = (BYTE)(((ex0 * (c00 & 0xFF)) + (ex1 * (c10 & 0xFF))) >> 16);
                b1 = (BYTE)(((ex0 * (c01 & 0xFF)) + (ex1 * (c11 & 0xFF))) >> 16);
                g0 = (BYTE)(((ex0 * ((c00 >> 8) & 0xFF)) + (ex1 * ((c10 >> 8) & 0xFF))) >> 16);
                g1 = (BYTE)(((ex0 * ((c01 >> 8) & 0xFF)) + (ex1 * ((c11 >> 8) & 0xFF))) >> 16);
                r0 = (BYTE)(((ex0 * ((c00 >> 16) & 0xFF)) + (ex1 * ((c10 >> 16) & 0xFF))) >> 16);
                r1 = (BYTE)(((ex0 * ((c01 >> 16) & 0xFF)) + (ex1 * ((c11 >> 16) & 0xFF))) >> 16);
                a0 = (BYTE)(((ex0 * ((c00 >> 24) & 0xFF)) + (ex1 * ((c10 >> 24) & 0xFF))) >> 16);
                a1 = (BYTE)(((ex0 * ((c01 >> 24) & 0xFF)) + (ex1 * ((c11 >> 24) & 0xFF))) >> 16);
                b0 = (BYTE)((ey0 * b0 + ey1 * b1) >> 16);
                g0 = (BYTE)((ey0 * g0 + ey1 * g1) >> 16);
                r0 = (BYTE)((ey0 * r0 + ey1 * r1) >> 16);
                a0 = (BYTE)((ey0 * a0 + ey1 * a1) >> 16);
                *(DWORD *)&pbBits[mx * 4 + my * widthbytes] =
                    MAKELONG(MAKEWORD(b0, g0), MAKEWORD(r0, a0));
            }
            x += cost << 8;
            y -= sint << 8;
        }
        x -= cx * cost << 8;
        x += sint << 8;
        y -= -cx * sint << 8;
        y += cost << 8;
    }
    HeapFree(GetProcessHeap(), 0, pbBitsSrc);
    DeleteDC(hdc);
    return hbm;
}

HBITMAP LoadBitmapFromFile(LPCTSTR pszFileName, float *dpi)
{
    HANDLE hFile;
    BITMAPFILEHEADER bf;
    BITMAPINFOEX bi;
    DWORD cb, cbImage;
    DWORD dwError;
    LPVOID pBits, pBits2;
    HDC hDC, hMemDC;
    HBITMAP hbm;

    hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return NULL;

    if (!ReadFile(hFile, &bf, sizeof(BITMAPFILEHEADER), &cb, NULL))
    {
        dwError = GetLastError();
        CloseHandle(NULL);
        SetLastError(dwError);
        return NULL;
    }

    pBits = NULL;
    if (bf.bfType == 0x4D42 && bf.bfReserved1 == 0 && bf.bfReserved2 == 0 &&
        bf.bfSize > bf.bfOffBits && bf.bfOffBits > sizeof(BITMAPFILEHEADER) &&
        bf.bfOffBits <= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOEX))
    {
        if (ReadFile(hFile, &bi, bf.bfOffBits -
                     sizeof(BITMAPFILEHEADER), &cb, NULL))
        {
#ifndef LR_LOADREALSIZE
#define LR_LOADREALSIZE 128
#endif
            *dpi = bi.bmiHeader.biXPelsPerMeter * 2.54 / 100.0;
            hbm = (HBITMAP)LoadImage(NULL, pszFileName, IMAGE_BITMAP, 
                0, 0, LR_LOADFROMFILE | LR_LOADREALSIZE | 
                LR_CREATEDIBSECTION);
            if (hbm != NULL)
            {
                CloseHandle(hFile);
                return hbm;
            }
            cbImage = bf.bfSize - bf.bfOffBits;
            pBits = HeapAlloc(GetProcessHeap(), 0, cbImage);
            if (pBits != NULL)
            {
                if (ReadFile(hFile, pBits, cbImage, &cb, NULL))
                {
                    ;
                }
                else
                {
                    dwError = GetLastError();
                    HeapFree(GetProcessHeap(), 0, pBits);
                    pBits = NULL;
                }
            }
            else
                dwError = GetLastError();
        }
        else
            dwError = GetLastError();
    }
    else
        dwError = ERROR_INVALID_DATA;
    CloseHandle(hFile);

    if (pBits == NULL)
    {
        SetLastError(dwError);
        return NULL;
    }

    hbm = NULL;
    hDC = GetDC(NULL);
    if (hDC != NULL)
    {
        hMemDC = CreateCompatibleDC(hDC);
        if (hMemDC != NULL)
        {
            hbm = CreateDIBSection(hMemDC, (BITMAPINFO*)&bi, DIB_RGB_COLORS,
                                   &pBits2, NULL, 0);
            if (hbm != NULL)
            {
                if (SetDIBits(hMemDC, hbm, 0, abs(bi.bmiHeader.biHeight),
                              pBits, (BITMAPINFO*)&bi, DIB_RGB_COLORS))
                {
                    ;
                }
                else
                {
                    dwError = GetLastError();
                    DeleteObject(hbm);
                    hbm = NULL;
                }
            }
            else
                dwError = GetLastError();

            DeleteDC(hMemDC);
        }
        else
            dwError = GetLastError();

        ReleaseDC(NULL, hDC);
    }
    else
        dwError = GetLastError();

    HeapFree(GetProcessHeap(), 0, pBits);
    SetLastError(dwError);

    return hbm;
}

BOOL SaveBitmapToFile(LPCTSTR pszFileName, HBITMAP hbm, float dpi)
{
    BOOL f;
    DWORD dwError;
    BITMAPFILEHEADER bf;
    BITMAPINFOEX bi;
    BITMAPINFOHEADER *pbmih;
    DWORD cb;
    DWORD cColors, cbColors;
    HDC hDC;
    HANDLE hFile;
    VOID *pBits;
    BITMAP bm;

    if (!GetObject(hbm, sizeof(BITMAP), &bm))
        return FALSE;

    pbmih = &bi.bmiHeader;
    ZeroMemory(pbmih, sizeof(BITMAPINFOHEADER));
    pbmih->biSize             = sizeof(BITMAPINFOHEADER);
    pbmih->biWidth            = bm.bmWidth;
    pbmih->biHeight           = bm.bmHeight;
    pbmih->biPlanes           = 1;
    pbmih->biBitCount         = bm.bmBitsPixel;
    pbmih->biCompression      = BI_RGB;
    pbmih->biSizeImage        = bm.bmWidthBytes * bm.bmHeight;
    if (dpi != 0.0)
    {
        pbmih->biXPelsPerMeter = pbmih->biYPelsPerMeter = dpi * 100 / 2.54 + 0.5;
    }

    if (bm.bmBitsPixel < 16)
        cColors = 1 << bm.bmBitsPixel;
    else
        cColors = 0;
    cbColors = cColors * sizeof(RGBQUAD);

    bf.bfType = 0x4d42;
    bf.bfReserved1 = 0;
    bf.bfReserved2 = 0;
    cb = sizeof(BITMAPFILEHEADER) + pbmih->biSize + cbColors;
    bf.bfOffBits = cb;
    bf.bfSize = cb + pbmih->biSizeImage;

    pBits = HeapAlloc(GetProcessHeap(), 0, pbmih->biSizeImage);
    if (pBits == NULL)
        return FALSE;

    f = FALSE;
    hDC = GetDC(NULL);
    if (hDC != NULL)
    {
        if (GetDIBits(hDC, hbm, 0, bm.bmHeight, pBits, (BITMAPINFO*)&bi, 
            DIB_RGB_COLORS))
        {
            hFile = CreateFile(pszFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | 
                               FILE_FLAG_WRITE_THROUGH, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                f = WriteFile(hFile, &bf, sizeof(BITMAPFILEHEADER), &cb, NULL) &&
                    WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &cb, NULL) &&
                    WriteFile(hFile, bi.bmiColors, cbColors, &cb, NULL) &&
                    WriteFile(hFile, pBits, pbmih->biSizeImage, &cb, NULL);
                if (!f)
                    dwError = GetLastError();
                CloseHandle(hFile);

                if (!f)
                    DeleteFile(pszFileName);
            }
            else
                dwError = GetLastError();
        }
        else
            dwError = GetLastError();
        ReleaseDC(NULL, hDC);
    }
    else
        dwError = GetLastError();

    HeapFree(GetProcessHeap(), 0, pBits);
    SetLastError(dwError);
    return f;
}

HBITMAP CopyBitmap(HBITMAP hbm)
{
    return (HBITMAP)CopyImage(hbm, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
}

HBITMAP CreateSolid32BppBitmap(INT cx, INT cy, COLORREF clr)
{
    BITMAPINFO bi;
    HDC hdc;
    HBITMAP hbm;
    HGDIOBJ hbmOld;
    VOID *pvBits;
    RECT rc;
    HBRUSH hbr;
    DWORD cdw;
    BYTE *pb;

    hdc = CreateCompatibleDC(NULL);
    if (hdc == NULL)
        return NULL;

    hbr = CreateSolidBrush(clr);
    if (hbr == NULL)
    {
        DeleteDC(hdc);
        return NULL;
    }

    ZeroMemory(&bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = cx;
    bi.bmiHeader.biHeight   = cy;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (hbm != NULL)
    {
        rc.left = 0;
        rc.top = 0;
        rc.right = cx;
        rc.bottom = cy;
        hbmOld = SelectObject(hdc, hbm);
        FillRect(hdc, &rc, hbr);
        SelectObject(hdc, hbmOld);
        cdw = cx * cy;
        pb = (BYTE *)pvBits;
        while(cdw--)
        {
            pb++;
            pb++;
            pb++;
            *pb++ = 0xFF;
        }
    }
    DeleteObject(hbr);
    DeleteDC(hdc);
    return hbm;
}

BOOL IsDIBOpaque(HBITMAP hbm)
{
    DWORD cdw;
    BYTE *pb;
    BITMAP bm;
    GetObject(hbm, sizeof(BITMAP), &bm);
    if (bm.bmBitsPixel <= 24)
        return TRUE;
    cdw = bm.bmWidth * bm.bmHeight;
    pb = (BYTE *)bm.bmBits;
    while(cdw--)
    {
        pb++;
        pb++;
        pb++;
        if (*pb++ != 0xFF)
            return FALSE;
    }
    return TRUE;
}

BOOL AlphaBlendBitmap(HBITMAP hbm1, HBITMAP hbm2)
{
    BITMAP bm1, bm2;
    BYTE *pb1, *pb2;
    DWORD cdw;
    INT x, y;
    BYTE a1, a2;
    GetObject(hbm1, sizeof(BITMAP), &bm1);
    GetObject(hbm2, sizeof(BITMAP), &bm2);

    if (bm1.bmBitsPixel == 32 && bm2.bmBitsPixel == 32)
    {
        pb1 = (BYTE *)bm1.bmBits;
        pb2 = (BYTE *)bm2.bmBits;
        cdw = bm1.bmWidth * bm1.bmHeight;
        while(cdw--)
        {
            a2 = pb2[3];
            a1 = (BYTE)(255 - a2);
            *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
            *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
            *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
            *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
        }
        return TRUE;
    }

    if (bm1.bmBitsPixel == 24 && bm2.bmBitsPixel == 32)
    {
        pb1 = (BYTE *)bm1.bmBits;
        pb2 = (BYTE *)bm2.bmBits;
        for(y = 0; y < bm1.bmHeight; y++)
        {
            for(x = 0; x < bm1.bmWidth; x++)
            {
                a2 = pb2[3];
                a1 = (BYTE)(255 - a2);
                *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
                *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
                *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
                pb2++;
            }
            pb1 += bm1.bmWidthBytes - bm1.bmWidth * 3;
        }
        return TRUE;
    }
    return FALSE;
}
