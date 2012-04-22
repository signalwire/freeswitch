/*---------------------------------------------------------------------------*\

  FILE........: lsp.c
  AUTHOR......: David Rowe
  DATE CREATED: 24/2/93


  This file contains functions for LPC to LSP conversion and LSP to
  LPC conversion. Note that the LSP coefficients are not in radians
  format but in the x domain of the unit circle.

\*---------------------------------------------------------------------------*/

#ifndef __LSP__
#define __LSP__

int lpc_to_lsp (float *a, int lpcrdr, float *freq, int nb, float delta);
void lsp_to_lpc(float *freq, float *ak, int lpcrdr);

#endif
