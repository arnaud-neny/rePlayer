/*
    MPEG-4 Audio RM Module
    Lossless coding of 1-bit oversampled audio - DST (Direct Stream Transfer)

    This software was originally developed by:

    * Aad Rijnberg
    Philips Digital Systems Laboratories Eindhoven
    <aad.rijnberg@philips.com>

    * Fons Bruekers
    Philips Research Laboratories Eindhoven
    <fons.bruekers@philips.com>

    * Eric Knapen
    Philips Digital Systems Laboratories Eindhoven
    <h.w.m.knapen@philips.com>

    And edited by:

    * Richard Theelen
    Philips Digital Systems Laboratories Eindhoven
    <r.h.m.theelen@philips.com>

    * Maxim V.Anisiutkin
    <maxim.anisiutkin@gmail.com>

    * Robert Tari
    <robert@tari.in>

    in the course of development of the MPEG-4 Audio standard ISO-14496-1, 2 and 3.
    This software module is an implementation of a part of one or more MPEG-4 Audio
    tools as specified by the MPEG-4 Audio standard. ISO/IEC gives users of the
    MPEG-4 Audio standards free licence to this software module or modifications
    thereof for use in hardware or software products claiming conformance to the
    MPEG-4 Audio standards. Those intending to use this software module in hardware
    or software products are advised that this use may infringe existing patents.
    The original developers of this software of this module and their company,
    the subsequent editors and their companies, and ISO/EIC have no liability for
    use of this software module or modifications thereof in an implementation.
    Copyright is not released for non MPEG-4 Audio conforming products. The
    original developer retains full right to use this code for his/her own purpose,
    assign or donate the code to a third party and to inhibit third party from
    using the code for non MPEG-4 Audio conforming products. This copyright notice
    must be included in all copies of derivative works.

    Copyright © 2004-2019.

    This file is part of SACD.

    SACD is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SACD is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SACD.  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
*/

#include "coded_table.h"

CCodedTable::CCodedTable()
{
    for (int i = 0; i < NROFFRICEMETHODS; i++)
    {
        for (int j = 0; j < MAXCPREDORDER; j++)
        {
            CPredCoef[i][j] = 0;
        }
    }
}

CCodedTableF::CCodedTableF()
{
    CPredOrder[0] = 1;
    CPredCoef[0][0] = -8;
    CPredOrder[1] = 2;
    CPredCoef[1][0] = -16;
    CPredCoef[1][1] = 8;
    CPredOrder[2] = 3;
    CPredCoef[2][0] = -9;
    CPredCoef[2][1] = -5;
    CPredCoef[2][2] = 6;
#if NROFFRICEMETHODS == 4
    CPredOrder[3] = 1;
    CPredCoef[3][0] = 8;
#endif
}

CCodedTableP::CCodedTableP()
{
    CPredOrder[0] = 1;
    CPredCoef[0][0] = -8;
    CPredOrder[1] = 2;
    CPredCoef[1][0] = -16;
    CPredCoef[1][1] = 8;
    CPredOrder[2] = 3;
    CPredCoef[2][0] = -24;
    CPredCoef[2][1] = 24;
    CPredCoef[2][2] = -8;
}
