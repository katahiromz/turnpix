#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <dlgs.h>
#include <tchar.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "turnpix.h"
#include "resource.h"

static const TCHAR g_szWndClass[] = TEXT("TurnPix Wnd Class");
static const TCHAR g_szCanvasWndClass[] = TEXT("TurnPix Canvas Wnd Class");

HINSTANCE g_hInstance;
HWND g_hMainWnd;
HWND g_hCanvasWnd;
HWND g_hStatusBar;
HACCEL g_hAccel;

TCHAR g_szFileName[MAX_PATH] = TEXT("");
INT g_iImageType = BMP;
TCHAR g_szInputFilter[1024];
TCHAR g_szOutputFilter[1024];

BOOL g_fGrow = TRUE;
double g_eAngle, g_eAngle2;
SIZE g_sizOriginal;
HBITMAP g_hbmOriginal = NULL;
SIZE g_sizRotated;
HBITMAP g_hbmRotated = NULL;
COLORREF g_clrBack = CLR_INVALID;

INT g_nZoomPercent = 100;
INT g_xScrollPos = -1;
INT g_yScrollPos = -1;

COLORREF g_clrTransparent = CLR_INVALID;

BOOL g_fCaptured;
POINT g_pt0, g_pt1;

float g_dpi;  /* dots per inch */

LPCTSTR LoadStringDx(INT id)
{
    static TCHAR sz[1024];
    LoadString(g_hInstance, id, sz, 1024);
    return sz;
}

void RemoveExtension(LPTSTR pszFileName)
{
    LPTSTR pch;
    pch = _tcsrchr(pszFileName, '\\');
    if (pch == NULL)
        return;
    pch = strrchr(pch, '.');
    if (pch == NULL)
        return;
    *pch = 0;
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

VOID UpdateWindowCaption(VOID)
{
    TCHAR szFileTitle[MAX_PATH];
    TCHAR szCaption[1024];

    if (g_szFileName[0] != 0)
    {
        GetFileTitle(g_szFileName, szFileTitle, MAX_PATH);
        wsprintf(szCaption, LoadStringDx(2), szFileTitle);
    }
    else
    {
        lstrcpy(szCaption, LoadStringDx(1));
    }
    SetWindowText(g_hMainWnd, szCaption);
}

VOID SetFileName(LPCTSTR pszFileName)
{
    lstrcpyn(g_szFileName, pszFileName, MAX_PATH);
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

void Canvas_OnPaint(HWND hWnd, HDC hdc, RECT *prcPaint)
{
    RECT rc;
    SIZE siz;
    INT x, y;
    HBITMAP hbm;
    HDC hdc3;
    HGDIOBJ hbm3Old;

    if (g_hbmOriginal == NULL)
        return;

    hbm = CreateBackBitmap();
    if (hbm != NULL)
    {
        GetClientRect(hWnd, &rc);
        siz.cx = rc.right - rc.left;
        siz.cy = rc.bottom - rc.top;
        AlphaBlendBitmap(hbm, g_hbmRotated);

        if (g_xScrollPos == -1)
            x = (siz.cx - g_sizRotated.cx * g_nZoomPercent / 100) / 2;
        else
            x = -g_xScrollPos;
        if (g_yScrollPos == -1)
            y = (siz.cy - g_sizRotated.cy * g_nZoomPercent / 100) / 2;
        else
            y = -g_yScrollPos;

        hdc3 = CreateCompatibleDC(NULL);
        hbm3Old = SelectObject(hdc3, hbm);
        SetStretchBltMode(hdc, COLORONCOLOR);
        StretchBlt(hdc,
            x,
            y,
            g_sizRotated.cx * g_nZoomPercent / 100,
            g_sizRotated.cy * g_nZoomPercent / 100,
            hdc3,
            0,
            0,
            g_sizRotated.cx,
            g_sizRotated.cy,
            SRCCOPY);
        SelectObject(hdc3, hbm3Old);
        DeleteDC(hdc3);
        DeleteObject(hbm);
    }
}

void Canvas_OnSize(HWND hWnd)
{
    RECT rc;
    SIZE siz, siz2;
    GetClientRect(hWnd, &rc);
    siz.cx = rc.right - rc.left;
    siz.cy = rc.bottom - rc.top;
    siz2.cx = g_sizRotated.cx * g_nZoomPercent / 100;
    siz2.cy = g_sizRotated.cy * g_nZoomPercent / 100;
    if (siz.cx < siz2.cx)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = siz2.cx;
        si.nPage = siz.cx;
        g_xScrollPos = si.nPos = (siz2.cx - siz.cx) / 2;
        SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
        ShowScrollBar(hWnd, SB_HORZ, TRUE);
    }
    else
    {
        g_xScrollPos = -1;
        ShowScrollBar(hWnd, SB_HORZ, FALSE);
    }
    if (siz.cy < siz2.cy)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = siz2.cy;
        si.nPage = siz.cy;
        g_yScrollPos = si.nPos = (siz2.cy - siz.cy) / 2;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        ShowScrollBar(hWnd, SB_VERT, TRUE);
    }
    else
    {
        g_yScrollPos = -1;
        ShowScrollBar(hWnd, SB_VERT, FALSE);
    }
    InvalidateRect(hWnd, NULL, TRUE);
}

void OnHScroll(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    SIZE siz, siz2;
    INT nMin, nMax, nPos, nDelta;
    GetClientRect(hWnd, &rc);
    siz.cx = rc.right - rc.left;
    siz.cy = rc.bottom - rc.top;
    GetScrollRange(hWnd, SB_HORZ, &nMin, &nMax);
    nPos = GetScrollPos(hWnd, SB_HORZ);
    switch(LOWORD(wParam))
    {
    case SB_LINELEFT:
        nPos -= 10;
        if (nPos < nMin)
            nPos = nMin;
        break;

    case SB_LINERIGHT:
        nPos += 10;
        if (nMax - siz.cx + 1 < nPos)
            nPos = nMax - siz.cx + 1;
        break;

    case SB_PAGELEFT:
        nPos -= siz.cx;
        if (nPos < nMin)
            nPos = nMin;
        break;

    case SB_PAGERIGHT:
        nPos += siz.cx;
        if (nMax - siz.cx + 1 < nPos)
            nPos = nMax - siz.cx + 1;
        break;

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        nPos = (SHORT)HIWORD(wParam);
        break;
    }
    nDelta = nPos - g_xScrollPos;
    if (abs(nDelta) < siz.cx)
        ScrollWindow(hWnd, -nDelta, 0, &rc, NULL);
    if (nDelta > 0)
    {
        rc.left = rc.right - nDelta;
    }
    else
    {
        rc.right = rc.left - nDelta;
    }
    g_xScrollPos = nPos;
    SetScrollPos(hWnd, SB_HORZ, nPos, TRUE);
    if (abs(nDelta) < siz.cx)
        InvalidateRect(hWnd, &rc, TRUE);
    else
        InvalidateRect(hWnd, NULL, TRUE);
}

void OnVScroll(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    SIZE siz, siz2;
    INT nMin, nMax, nPos, nDelta;
    GetClientRect(hWnd, &rc);
    siz.cx = rc.right - rc.left;
    siz.cy = rc.bottom - rc.top;
    GetScrollRange(hWnd, SB_VERT, &nMin, &nMax);
    nPos = GetScrollPos(hWnd, SB_VERT);
    switch(LOWORD(wParam))
    {
    case SB_LINEUP:
        nPos -= 10;
        if (nPos < nMin)
            nPos = nMin;
        break;

    case SB_LINEDOWN:
        nPos += 10;
        if (nMax - siz.cy + 1 < nPos)
            nPos = nMax - siz.cy + 1;
        break;

    case SB_PAGEUP:
        nPos -= siz.cy;
        if (nPos < nMin)
            nPos = nMin;
        break;

    case SB_PAGEDOWN:
        nPos += siz.cy;
        if (nMax - siz.cy + 1 < nPos)
            nPos = nMax - siz.cy + 1;
        break;

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        nPos = (SHORT)HIWORD(wParam);
        break;
    }
    nDelta = nPos - g_yScrollPos;
    if (abs(nDelta) < siz.cy)
        ScrollWindow(hWnd, 0, -nDelta, &rc, NULL);
    if (nDelta > 0)
    {
        rc.top = rc.bottom - nDelta;
    }
    else
    {
        rc.bottom = rc.top - nDelta;
    }
    g_yScrollPos = nPos;
    SetScrollPos(hWnd, SB_VERT, nPos, TRUE);
    if (abs(nDelta) < siz.cy)
        InvalidateRect(hWnd, &rc, TRUE);
    else
        InvalidateRect(hWnd, NULL, TRUE);
}

void RotatePoints(POINT *ppt, INT cpt, double angle)
{
    INT x, y;
    while(cpt--)
    {
        x = ppt->x;
        y = ppt->y;
        ppt->x = x * cos(angle) + y * sin(angle);
        ppt->y = - x * sin(angle) + y * cos(angle);
        ppt++;
    }
}

void OffsetPoints(POINT *ppt, INT cpt, INT dx, INT dy)
{
    while(cpt--)
    {
        ppt->x += dx;
        ppt->y += dy;
        ppt++;
    }
}

void OnChange(void)
{
    HBITMAP hbm;
    HCURSOR hcurWait = LoadCursor(NULL, IDC_WAIT);
    HCURSOR hcurOld = SetCursor(hcurWait);
    hbm = CreateRotated32BppBitmap(g_hbmOriginal, g_eAngle, g_fGrow);
    if (hbm != NULL)
    {
        BITMAP bm;
        TCHAR sz[256];
        DeleteObject(g_hbmRotated);
        g_hbmRotated = hbm;
        GetObject(hbm, sizeof(BITMAP), &bm);
        g_sizRotated.cx = bm.bmWidth;
        g_sizRotated.cy = bm.bmHeight;
        InvalidateRect(g_hCanvasWnd, NULL, TRUE);
        _stprintf(sz, LoadStringDx(15), g_eAngle * 180.0 / M_PI, g_nZoomPercent);
        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)sz);
        SendMessage(g_hCanvasWnd, WM_SIZE, 0, 0);
    }
    SetCursor(hcurOld);
}

void DrawGuide(HWND hWnd)
{
    RECT rc;
    SIZE siz;
    HDC hdc;
    POINT apt[4];
    INT x0, y0;
    double angle, angle0, angle1;

    GetClientRect(hWnd, &rc);
    siz.cx = rc.right - rc.left;
    siz.cy = rc.bottom - rc.top;
    hdc = GetDC(hWnd);
    SelectObject(hdc, GetStockObject(WHITE_PEN));
    SetROP2(hdc, R2_XORPEN);
    if (g_xScrollPos == -1)
        x0 = siz.cx / 2;
    else
        x0 = g_sizRotated.cx * g_nZoomPercent / 100 / 2 - g_xScrollPos;
    if (g_yScrollPos == -1)
        y0 = siz.cy / 2;
    else
        y0 = g_sizRotated.cy * g_nZoomPercent / 100 / 2 - g_yScrollPos;
    MoveToEx(hdc, x0, y0, NULL);
    LineTo(hdc, g_pt1.x, g_pt1.y);
    apt[0].x = - g_sizOriginal.cx * g_nZoomPercent / 100 / 2;
    apt[0].y = - g_sizOriginal.cy * g_nZoomPercent / 100 / 2;
    apt[1].x =   g_sizOriginal.cx * g_nZoomPercent / 100 / 2;
    apt[1].y = - g_sizOriginal.cy * g_nZoomPercent / 100 / 2;
    apt[2].x =   g_sizOriginal.cx * g_nZoomPercent / 100 / 2;
    apt[2].y =   g_sizOriginal.cy * g_nZoomPercent / 100 / 2;
    apt[3].x = - g_sizOriginal.cx * g_nZoomPercent / 100 / 2;
    apt[3].y =   g_sizOriginal.cy * g_nZoomPercent / 100 / 2;
    angle0 = atan2(g_pt0.x - x0, g_pt0.y - y0);
    angle1 = atan2(g_pt1.x - x0, g_pt1.y - y0);
    angle = angle1 - angle0;
    g_eAngle2 = fmod(angle + M_PI * 2.0, M_PI * 2.0);
    RotatePoints(apt, 4, g_eAngle + g_eAngle2);
    OffsetPoints(apt, 4, x0, y0);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polygon(hdc, apt, 4);
    ReleaseDC(hWnd, hdc);
}

void OnLButtonDown(HWND hWnd, INT x, INT y, UINT keyFlags)
{
    POINT pt;
    if (g_hbmOriginal != NULL)
    {
        g_pt0.x = g_pt1.x = x;
        g_pt0.y = g_pt1.y = y;
        DrawGuide(hWnd);
        SetCapture(hWnd);
        g_fCaptured = TRUE;
    }
}

void OnLButtonUp(HWND hWnd, INT x, INT y, UINT keyFlags)
{
    POINT pt;
    if (GetCapture() == hWnd)
    {
        g_pt1.x = x;
        g_pt1.y = y;
        DrawGuide(hWnd);
        ReleaseCapture();
        g_fCaptured = FALSE;
        g_eAngle += g_eAngle2;
        g_eAngle = fmod(g_eAngle + M_PI * 2.0, M_PI * 2.0);
        OnChange();
    }
}

void OnMouseMove(HWND hWnd, INT x, INT y, UINT keyFlags)
{
    POINT pt;
    TCHAR sz[256];
    if (g_fCaptured)
    {
        DrawGuide(hWnd);
        g_pt1.x = x;
        g_pt1.y = y;
        DrawGuide(hWnd);
        _stprintf(sz, LoadStringDx(15), (g_eAngle + g_eAngle2) * 180.0 / M_PI, 
                  g_nZoomPercent);
        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)sz);
    }
}

LRESULT CALLBACK CanvasWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    PAINTSTRUCT ps;
    HDC hdc;
    switch(uMsg)
    {
    case WM_SIZE:
        Canvas_OnSize(hWnd);
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        if (hdc != NULL)
        {
            Canvas_OnPaint(hWnd, hdc, &ps.rcPaint);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_LBUTTONDOWN:
        OnLButtonDown(hWnd, (INT)(SHORT)LOWORD(lParam), (INT)(SHORT)HIWORD(lParam), (UINT)(wParam));
        break;

    case WM_LBUTTONUP:
        OnLButtonUp(hWnd, (INT)(SHORT)LOWORD(lParam), (INT)(SHORT)HIWORD(lParam), (UINT)(wParam));
        break;

    case WM_MOUSEMOVE:
        OnMouseMove(hWnd, (INT)(SHORT)LOWORD(lParam), (INT)(SHORT)HIWORD(lParam), (UINT)(wParam));
        break;

    case WM_CAPTURECHANGED:
        g_fCaptured = FALSE;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_HSCROLL:
        OnHScroll(hWnd, wParam, lParam);
        break;

    case WM_VSCROLL:
        OnVScroll(hWnd, wParam, lParam);
        break;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

BOOL DoOpenFile(LPCTSTR pszFileName)
{
    HBITMAP hbm;
    float dpi;
    HCURSOR hcurWait = LoadCursor(NULL, IDC_WAIT);
    HCURSOR hcurOld = SetCursor(hcurWait);
    INT i = GetImageType(pszFileName);
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
        BITMAP bm;
        g_dpi = dpi;
        if (g_hbmOriginal != NULL)
            DeleteObject(g_hbmOriginal);
        g_hbmOriginal = hbm;
        GetObject(hbm, sizeof(BITMAP), &bm);
        g_sizOriginal.cx = bm.bmWidth;
        g_sizOriginal.cy = bm.bmHeight;
        if (g_hbmRotated != NULL)
            DeleteObject(g_hbmRotated);
        g_hbmRotated = CopyBitmap(g_hbmOriginal);
        g_sizRotated = g_sizOriginal;
        g_nZoomPercent = 100;
        g_iImageType = i;
        g_eAngle = 0;
        g_fGrow = TRUE;
        g_clrBack = CLR_INVALID;
        SetFileName(pszFileName);
        UpdateWindowCaption();
        OnChange();
        SetCursor(hcurOld);
        return TRUE;
    }
    SetCursor(hcurOld);
    MessageBox(g_hMainWnd, LoadStringDx(17), NULL, MB_ICONERROR);
    return FALSE;
}

UINT APIENTRY CCHookProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CHOOSECOLOR *pcc;
    switch(uMsg)
    {
    case WM_INITDIALOG:
        pcc = (CHOOSECOLOR *)lParam;
        SetWindowText(hDlg, (LPCTSTR)pcc->lCustData);
        return 1;
    }
    return 0;
}

BOOL ChooseColorDx(COLORREF *pclrChoosed, LPCTSTR pszTitle)
{
    CHOOSECOLOR cc;
    static COLORREF aCustColors[16] =
    {
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255)
    };

    ZeroMemory(&cc, sizeof(CHOOSECOLOR));
    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner   = g_hMainWnd;
    cc.rgbResult = *pclrChoosed;
    cc.lpCustColors = aCustColors;
    cc.lpfnHook    = CCHookProc;
    cc.Flags       = CC_RGBINIT | CC_ENABLEHOOK;
    cc.lCustData   = (LPARAM)pszTitle;

    *pclrChoosed = CLR_INVALID;
    if (ChooseColor(&cc))
    {
        *pclrChoosed = cc.rgbResult;
        return TRUE;
    }
    return FALSE;
}

BOOL DoSaveFile(LPCTSTR pszFileName)
{
    BOOL f;
    INT i;
    HBITMAP hbm;
    COLORREF clr = g_clrBack;
    HCURSOR hcurWait = LoadCursor(NULL, IDC_WAIT);
    HCURSOR hcurOld = SetCursor(hcurWait);

    i = GetImageType(pszFileName);
    if (i == GIF)
    {
        if (MessageBox(g_hMainWnd, LoadStringDx(19), NULL, 
                       MB_ICONWARNING | MB_YESNO) == IDNO)
        {
            SetCursor(hcurOld);
            return FALSE;
        }
    }

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
        else if (ChooseColorDx(&g_clrTransparent, LoadStringDx(21)))
        {
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
    if (!f)
        MessageBox(g_hMainWnd, LoadStringDx(18), NULL, MB_ICONERROR);

    SetCursor(hcurOld);
    return f;
}

BOOL DoCloseFile(VOID)
{
    if (g_eAngle != 0.0)
    {
        SetFileName("");
        UpdateWindowCaption();
    }
    return TRUE;
}

BOOL Open(void)
{
    OPENFILENAME ofn;
    TCHAR szFileName[MAX_PATH];

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    lstrcpyn(szFileName, g_szFileName, MAX_PATH);
    ofn.lStructSize     = sizeof(OPENFILENAME);
    ofn.hwndOwner       = g_hMainWnd;
    ofn.lpstrFilter     = g_szInputFilter;
    ofn.nFilterIndex    = 1;
    ofn.lpstrFile       = szFileName;
    ofn.nMaxFile        = MAX_PATH;
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
        OFN_HIDEREADONLY;
    ofn.lpstrDefExt     = "bmp";
    if (GetOpenFileName(&ofn))
    {
        return DoOpenFile(szFileName);
    }
    return FALSE;
}

BOOL SaveAs(void)
{
    OPENFILENAME ofn;
    TCHAR szFileName[MAX_PATH];

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    lstrcpyn(szFileName, g_szFileName, MAX_PATH);
    RemoveExtension(szFileName);
    ofn.lStructSize     = sizeof(OPENFILENAME);
    ofn.hwndOwner       = g_hMainWnd;
    ofn.lpstrFilter     = g_szOutputFilter;
    switch(g_iImageType)
    {
    case BMP:   ofn.nFilterIndex    = 1; break;
    case GIF:   ofn.nFilterIndex    = 2; break;
    case JPEG:  ofn.nFilterIndex    = 3; break;
    case PNG:   ofn.nFilterIndex    = 4; break;
    case TIFF:  ofn.nFilterIndex    = 5; break;
    }
    ofn.lpstrFile       = szFileName;
    ofn.nMaxFile        = MAX_PATH;
    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT |
                          OFN_HIDEREADONLY;
    ofn.lpstrDefExt     = TEXT("bmp");
    if (GetSaveFileName(&ofn))
    {
        return DoSaveFile(szFileName);
    }
    return FALSE;
}

BOOL Save(void)
{
    if (g_szFileName[0] == '\0')
        return SaveAs();
    else
        return DoSaveFile(g_szFileName);
}

double GetDlgItemDouble(HWND hDlg, INT id)
{
    TCHAR sz[128];
    GetDlgItemText(hDlg, id, sz, 128);
    return _ttof(sz);
}

void SetDlgItemDouble(HWND hDlg, INT id, double d)
{
    TCHAR sz[128];
    static const TCHAR format[] = "%.2lf";
    _stprintf(sz, format, d);
    SetDlgItemText(hDlg, id, sz);
}

BOOL CALLBACK AngleDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
        SetDlgItemDouble(hDlg, edt1, g_eAngle * 180.0 / M_PI);
        SendDlgItemMessage(hDlg, edt1, EM_SETSEL, 0, -1);
        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
            g_eAngle = GetDlgItemDouble(hDlg, edt1) * M_PI / 180.0;
            EndDialog(hDlg, IDOK);
            OnChange();
            break;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            break;
        }
        break;
    }
    return FALSE;
}

BOOL CALLBACK AboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
        MessageBeep(MB_ICONINFORMATION);
        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
            EndDialog(hDlg, IDOK);
            break;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            break;
        }
        break;
    }
    return FALSE;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc, rc2;
    TCHAR szFileName[MAX_PATH];
    COLORREF clr;

    switch(uMsg)
    {
    case WM_CREATE:
        g_hCanvasWnd = CreateWindowEx(WS_EX_CLIENTEDGE, g_szCanvasWndClass, "", 
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)1, g_hInstance, NULL);
        if (g_hCanvasWnd == NULL)
            return -1;
        g_hStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, 
            LoadStringDx(16), hWnd, 2);
        if (g_hStatusBar == NULL)
            return -1;
        DragAcceptFiles(hWnd, TRUE);
        if (__argc > 1)
        {
            DoOpenFile(__argv[1]);
        }
        break;

    case WM_DROPFILES:
        DragQueryFile((HDROP)wParam, 0, szFileName, MAX_PATH);
        DoOpenFile(szFileName);
        break;

    case WM_SIZE:
        SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
        GetClientRect(hWnd, &rc);
        GetWindowRect(g_hStatusBar, &rc2);
        MoveWindow(g_hCanvasWnd, rc.left, rc.top, rc.right - rc.left, 
            rc.bottom - rc.top - (rc2.bottom - rc2.top), 
            TRUE);
        InvalidateRect(g_hCanvasWnd, NULL, TRUE);
        break;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case ID_EXIT:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;

        case ID_OPEN:
            Open();
            break;

        case ID_SAVE:
            Save();
            break;

        case ID_SAVEAS:
            SaveAs();
            break;

        case ID_ZOOMIN:
            g_nZoomPercent *= 2;
            if (g_nZoomPercent > MAX_ZOOM_PERCENT)
                g_nZoomPercent = MAX_ZOOM_PERCENT;
            OnChange();
            break;

        case ID_ZOOMOUT:
            g_nZoomPercent /= 2;
            if (g_nZoomPercent < MIN_ZOOM_PERCENT)
                g_nZoomPercent = MIN_ZOOM_PERCENT;
            OnChange();
            break;

        case ID_ZOOM25:
            g_nZoomPercent = 25;
            OnChange();
            break;

        case ID_ZOOM50:
            g_nZoomPercent = 50;
            OnChange();
            break;

        case ID_ZOOM100:
            g_nZoomPercent = 100;
            OnChange();
            break;

        case ID_ZOOM150:
            g_nZoomPercent = 150;
            OnChange();
            break;

        case ID_ZOOM200:
            g_nZoomPercent = 200;
            OnChange();
            break;

        case ID_ANGLE:
            DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ANGLE), hWnd, 
                AngleDlgProc);
            OnChange();
            break;

        case ID_MINUS0_1:
            g_eAngle -= 0.1 * M_PI / 180.0;
            OnChange();
            break;

        case ID_PLUS0_1:
            g_eAngle += 0.1 * M_PI / 180.0;
            OnChange();
            break;

        case ID_MINUS1:
            g_eAngle -= 1.0 * M_PI / 180.0;
            OnChange();
            break;

        case ID_PLUS1:
            g_eAngle += 1.0 * M_PI / 180.0;
            OnChange();
            break;

        case ID_MINUS10:
            g_eAngle -= 10.0 * M_PI / 180.0;
            OnChange();
            break;

        case ID_PLUS10:
            g_eAngle += 10.0 * M_PI / 180.0;
            OnChange();
            break;

        case ID_MINUS30:
            g_eAngle -= 30.0 * M_PI / 180.0;
            OnChange();
            break;

        case ID_PLUS30:
            g_eAngle += 30.0 * M_PI / 180.0;
            OnChange();
            break;

        case ID_MINUS90:
            g_eAngle -= 90.0 * M_PI / 180.0;
            OnChange();
            break;

        case ID_PLUS90:
            g_eAngle += 90.0 * M_PI / 180.0;
            OnChange();
            break;

        case ID_GROW:
            g_fGrow = !g_fGrow;
            OnChange();
            break;

        case ID_BACK:
            if (ChooseColorDx(&g_clrBack, LoadStringDx(20)))
            {
                OnChange();
            }
            break;

        case ID_TRANS:
            if (g_clrBack == CLR_INVALID)
                g_clrBack = RGB(0xFF, 0xFF, 0xFF);
            else
                g_clrBack = CLR_INVALID;
            OnChange();
            break;

        case ID_ABOUT:
            DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUT), hWnd, 
                AboutDlgProc);
            break;
        }
        break;

    case WM_INITMENUPOPUP:
        switch(g_nZoomPercent)
        {
        case 25: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM25, 0); break;
        case 50: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM50, 0); break;
        case 100: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM100, 0); break;
        case 150: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM150, 0); break;
        case 200: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM200, 0); break;
        default: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, -1, 0); break;
        }
        if (g_szFileName[0] != 0)
        {
            EnableMenuItem((HMENU)wParam, ID_SAVE, MF_ENABLED);
            EnableMenuItem((HMENU)wParam, ID_SAVEAS, MF_ENABLED);
        }
        else
        {
            EnableMenuItem((HMENU)wParam, ID_SAVE, MF_GRAYED);
            EnableMenuItem((HMENU)wParam, ID_SAVEAS, MF_GRAYED);
        }
        if (g_fGrow)
            CheckMenuItem((HMENU)wParam, ID_GROW, MF_CHECKED);
        else
            CheckMenuItem((HMENU)wParam, ID_GROW, MF_UNCHECKED);
        if (g_clrBack == CLR_INVALID)
            CheckMenuItem((HMENU)wParam, ID_TRANS, MF_CHECKED);
        else
            CheckMenuItem((HMENU)wParam, ID_TRANS, MF_UNCHECKED);
        if (g_nZoomPercent >= MAX_ZOOM_PERCENT)
            EnableMenuItem((HMENU)wParam, ID_ZOOMIN, MF_GRAYED);
        else
            EnableMenuItem((HMENU)wParam, ID_ZOOMIN, MF_ENABLED);
        if (g_nZoomPercent <= MIN_ZOOM_PERCENT)
            EnableMenuItem((HMENU)wParam, ID_ZOOMOUT, MF_GRAYED);
        else
            EnableMenuItem((HMENU)wParam, ID_ZOOMOUT, MF_ENABLED);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

INT WINAPI WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       pszCmdLine,
    INT         nCmdShow)
{
    WNDCLASSEX wcx;
    MSG msg;
    BOOL f;
    LPTSTR pch;

    g_hInstance = hInstance;
    InitCommonControls();
    g_dpi = 0.0;

    pch = g_szInputFilter;
    lstrcpy(pch, LoadStringDx(3));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(4));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(5));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(6));
    pch += lstrlen(pch) + 1;
    *pch = 0;

    pch = g_szOutputFilter;
    lstrcpy(pch, LoadStringDx(7));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(8));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(9));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(10));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(11));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(12));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(13));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(14));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(22));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(23));
    pch += lstrlen(pch) + 1;
    *pch = 0;

    wcx.cbSize          = sizeof(WNDCLASSEX);
    wcx.style           = 0;
    wcx.lpfnWndProc     = WindowProc;
    wcx.cbClsExtra      = 0;
    wcx.cbWndExtra      = 0;
    wcx.hInstance       = hInstance;
    wcx.hIcon           = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wcx.hCursor         = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground   = (HBRUSH)(COLOR_3DFACE + 1);
    wcx.lpszMenuName    = MAKEINTRESOURCE(1);
    wcx.lpszClassName   = g_szWndClass;
    wcx.hIconSm         = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), 
        IMAGE_ICON, 
        GetSystemMetrics(SM_CXSMICON), 
        GetSystemMetrics(SM_CYSMICON), 0);
    if (!RegisterClassEx(&wcx))
        return 1;

    wcx.lpfnWndProc     = CanvasWndProc;
    wcx.lpszMenuName    = NULL;
    wcx.lpszClassName   = g_szCanvasWndClass;
    if (!RegisterClassEx(&wcx))
        return 1;

    g_hMainWnd = CreateWindow(g_szWndClass, LoadStringDx(1), 
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, 0, 
        500, 350, NULL, NULL, hInstance, NULL);
    if (g_hMainWnd == NULL)
        return 2;

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    g_hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(1));

    while((f = GetMessage(&msg, NULL, 0, 0)) != FALSE)
    {
        if (f == -1)
            return -1;
        if (TranslateAccelerator(g_hMainWnd, g_hAccel, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (INT)msg.wParam;
}
