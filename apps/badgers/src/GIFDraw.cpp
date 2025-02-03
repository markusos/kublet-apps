// Based off https://github.com/bitbank2/AnimatedGIF/blob/master/examples/TFT_eSPI_memory/GIFDraw.ino

#include "GIFDraw.h"
#include <cstdint>
#include <AnimatedGIF.h>
#include <TFT_eSPI.h>

#include "globals.h"

#define DISPLAY_WIDTH tft.width()
#define DISPLAY_HEIGHT tft.height()
#define BUFFER_SIZE 240 // Buffer one line of pixels at a time

uint16_t usTemp[1][BUFFER_SIZE];
bool dmaBuf = 0;

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, iCount;

  // Display bounds check and cropping
  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - pDraw->iX;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line
  if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    return;

  // Old image disposal
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  s = pDraw->pPixels;

  // Unroll the first pass to boost DMA performance
  // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
  if (iWidth <= BUFFER_SIZE)
    for (iCount = 0; iCount < iWidth; iCount++)
      usTemp[dmaBuf][iCount] = usPalette[*s++];
  else
    for (iCount = 0; iCount < BUFFER_SIZE; iCount++)
      usTemp[dmaBuf][iCount] = usPalette[*s++];

  tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
  tft.pushPixels(&usTemp[0][0], iCount);

  iWidth -= iCount;
  // Loop if pixel buffer smaller than width
  while (iWidth > 0)
  {
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    if (iWidth <= BUFFER_SIZE)
      for (iCount = 0; iCount < iWidth; iCount++)
        usTemp[dmaBuf][iCount] = usPalette[*s++];
    else
      for (iCount = 0; iCount < BUFFER_SIZE; iCount++)
        usTemp[dmaBuf][iCount] = usPalette[*s++];

    tft.pushPixels(&usTemp[0][0], iCount);
    iWidth -= iCount;
  }
}