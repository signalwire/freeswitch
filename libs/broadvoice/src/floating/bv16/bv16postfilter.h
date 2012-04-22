/*****************************************************************************/
/* BroadVoice(R)16 (BV16) Floating-Point ANSI-C Source Code                  */
/* Revision Date: August 19, 2009                                            */
/* Version 1.0                                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Copyright 2000-2009 Broadcom Corporation                                  */
/*                                                                           */
/* This software is provided under the GNU Lesser General Public License,    */
/* version 2.1, as published by the Free Software Foundation ("LGPL").       */
/* This program is distributed in the hope that it will be useful, but       */
/* WITHOUT ANY SUPPORT OR WARRANTY; without even the implied warranty of     */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LGPL for     */
/* more details.  A copy of the LGPL is available at                         */
/* http://www.broadcom.com/licenses/LGPLv2.1.php,                            */
/* or by writing to the Free Software Foundation, Inc.,                      */
/* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.                 */
/*****************************************************************************/


/*****************************************************************************
  postfilt.h : BV16 Post Filter

  $Log: bv16postfilter.h,v $
  Revision 1.1.1.1  2009/11/19 12:10:48  steveu
  Start from Broadcom's code

  Revision 1.1.1.1  2009/11/17 14:06:02  steveu
  start

******************************************************************************/

void postfilter(Float *s,   /* input : quantized speech signal         */
                int pp,   /* input : pitch period                    */
                Float *ma_a,
                Float *b_prv,
                int *pp_prv,
                Float *e);  /* output: enhanced speech signal          */
