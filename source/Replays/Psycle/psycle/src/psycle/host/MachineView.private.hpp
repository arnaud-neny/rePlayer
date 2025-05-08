///\file
///\brief machine view graphic operations for psycle::host::CChildView, private header
#pragma once
#include <psycle/host/detail/project.hpp>
#include <algorithm>
namespace psycle { namespace host {

		using namespace helpers;

		namespace {
			COLORREF inline rgb(int r, int g, int b) {
				return RGB(
					std::max(0, std::min(r, 255)),
					std::max(0, std::min(g, 255)),
					std::max(0, std::min(b, 255)));
			}
		}

		void CChildView::PrepareDrawAllMacVus()
		{
			for (int c=0; c<MAX_MACHINES-1; c++)
			{
				Machine* pMac = _pSong._pMachine[c];
				if (pMac)
				{
					RECT r;
					switch (pMac->_mode)
					{
					case MACHMODE_GENERATOR:
						r.left = pMac->_x+MachineCoords->dGeneratorVu.x;
						r.top = pMac->_y+MachineCoords->dGeneratorVu.y;
						r.right = r.left + MachineCoords->dGeneratorVu.width;
						r.bottom = r.top + MachineCoords->sGeneratorVu0.height;
						break;
					case MACHMODE_FX:
						r.left = pMac->_x+MachineCoords->dEffectVu.x;
						r.top = pMac->_y+MachineCoords->dEffectVu.y;
						r.right = r.left + MachineCoords->dEffectVu.width;
						r.bottom = r.top + MachineCoords->sEffectVu0.height;
						break;
					}
					InvalidateRect(&r,false);
				}
			}
		}
		void CChildView::DrawAllMachineVumeters(CDC *devc)
		{
			if (macView->draw_vus)
			{
				for (int c=0; c<MAX_MACHINES-1; c++)
				{
					Machine* pMac = _pSong._pMachine[c];
					if (pMac)
					{
						pMac->_volumeMaxCounterLife--;
						if ((pMac->_volumeDisplay > pMac->_volumeMaxDisplay)
							|| (pMac->_volumeMaxCounterLife <= 0))
						{
							pMac->_volumeMaxDisplay = pMac->_volumeDisplay-1;
							pMac->_volumeMaxCounterLife = 60;
						}
						DrawMachineVol(c, devc);
					}
				}
			}
		}

		void CChildView::DrawMachineVumeters(int c, CDC *devc)
		{
			if (macView->draw_vus)
			{
				Machine* pMac = _pSong._pMachine[c];
				if (pMac)
				{
					pMac->_volumeMaxCounterLife--;
					if ((pMac->_volumeDisplay > pMac->_volumeMaxDisplay)
						|| (pMac->_volumeMaxCounterLife <= 0))
					{
						pMac->_volumeMaxDisplay = pMac->_volumeDisplay-1;
						pMac->_volumeMaxCounterLife = 60;
					}
					DrawMachineVol(c, devc);
				}
			}
		}


		void CChildView::DrawMachineEditor(CDC *devc)
		{
			CBrush fillbrush(macView->polycolour);
			CBrush *oldbrush = devc->SelectObject(&fillbrush);
			if (macView->bBmpBkg) // Draw Background image
			{
				///\todo: Background should be part of the buffered memDC.
				CDC memDC;
				CBitmap* oldbmp;
				memDC.CreateCompatibleDC(devc);
				oldbmp=memDC.SelectObject(&macView->machinebkg);
				int bkgx = macView->bkgx;
				int bkgy = macView->bkgy;

				if ((CW > bkgx) || (CH > bkgy)) 
				{
					for (int cx=0; cx<CW; cx+=bkgx)
					{
						for (int cy=0; cy<CH; cy+=bkgy)
						{
							devc->BitBlt(cx,cy,bkgx,bkgy,&memDC,0,0,SRCCOPY);
						}
					}
				}
				else
				{
					devc->BitBlt(0,0,CW,CH,&memDC,0,0,SRCCOPY);
				}

				memDC.SelectObject(oldbmp);
				memDC.DeleteDC();

			}
			else // else fill with solid color
			{
				CRect rClient;
				GetClientRect(&rClient);
				devc->FillSolidRect(&rClient,macView->colour);
			}

			if (macView->wireaa)
			{
				
				// the shaded arrow colors will be multiplied by these values to convert them from grayscale to the
				// polygon color stated in the config.
				float deltaColR = ((macView->polycolour     & 0xFF) / 510.0) + .45;
				float deltaColG = ((macView->polycolour>>8  & 0xFF) / 510.0) + .45;
				float deltaColB = ((macView->polycolour>>16 & 0xFF) / 510.0) + .45;
			
				CPen linepen1( PS_SOLID, macView->wirewidth+(macView->wireaa*2), macView->wireaacolour);
				CPen linepen2( PS_SOLID, macView->wirewidth+(macView->wireaa), macView->wireaacolour2); 
				CPen linepen3( PS_SOLID, macView->wirewidth, macView->wirecolour); 
	 			CPen polyInnardsPen(PS_SOLID, 0, RGB(192 * deltaColR, 192 * deltaColG, 192 * deltaColB));
				CPen *oldpen = devc->SelectObject(&linepen1);
				CBrush *oldbrush = static_cast<CBrush*>(devc->SelectStockObject(NULL_BRUSH));

				// Draw wire [connections]
				for(int c=0;c<MAX_MACHINES;c++)
				{
					Machine *tmac=_pSong._pMachine[c];
					if(tmac)
					{
						int oriX=0;
						int oriY=0;
						switch (tmac->_mode)
						{
						case MACHMODE_GENERATOR:
							oriX = tmac->_x+(MachineCoords->sGenerator.width/2);
							oriY = tmac->_y+(MachineCoords->sGenerator.height/2);
							break;
						case MACHMODE_FX:
							oriX = tmac->_x+(MachineCoords->sEffect.width/2);
							oriY = tmac->_y+(MachineCoords->sEffect.height/2);
							break;

						case MACHMODE_MASTER:
							oriX = tmac->_x+(MachineCoords->sMaster.width/2);
							oriY = tmac->_y+(MachineCoords->sMaster.height/2);
							break;
						}

						for (int w=0; w<MAX_CONNECTIONS; w++)
						{
							if (tmac->outWires[w] && tmac->outWires[w]->Enabled())
							{
								int desX = 0;
								int desY = 0;
								Machine& pout = tmac->outWires[w]->GetDstMachine();
								switch (pout._mode)
								{
								case MACHMODE_GENERATOR:
									desX = pout._x+(MachineCoords->sGenerator.width/2);
									desY = pout._y+(MachineCoords->sGenerator.height/2);
									break;
								case MACHMODE_FX:
									desX = pout._x+(MachineCoords->sEffect.width/2);
									desY = pout._y+(MachineCoords->sEffect.height/2);
									break;

								case MACHMODE_MASTER:
									desX = pout._x+(MachineCoords->sMaster.width/2);
									desY = pout._y+(MachineCoords->sMaster.height/2);
									break;
								}
								
								int const f1 = (desX+oriX)/2;
								int const f2 = (desY+oriY)/2;

								double modX = double(desX-oriX);
								double modY = double(desY-oriY);
								double modT = sqrt(modX*modX+modY*modY);
								
								modX = modX/modT;
								modY = modY/modT;
								double slope = atan2(modY, modX);
								double altslope;
										
								int rtcol = 140+abs(helpers::math::round<int,float>(slope*32));

								altslope=slope;
								if(altslope<-1.05)  altslope -= 2 * (altslope + 1.05);
								if(altslope>2.10) altslope -= 2 * (altslope - 2.10);
								int ltcol = 140 + abs(helpers::math::round<int,float>((altslope - 2.10) * 32));

								altslope=slope;
								if(altslope>0.79)  altslope -= 2 * (altslope - 0.79);
								if(altslope<-2.36)  altslope -= 2 * (altslope + 2.36);
								int btcol = 240 - abs(helpers::math::round<int,float>((altslope-0.79) * 32));

								// brushes for the right side, left side, and bottom of the arrow (when pointed straight up).
								CBrush rtBrush(rgb(
									rtcol * deltaColR,
									rtcol * deltaColG,
									rtcol * deltaColB));
								CBrush ltBrush(rgb(
									ltcol * deltaColR,
									ltcol * deltaColG,
									ltcol * deltaColB));
								CBrush btBrush(rgb(
									btcol * deltaColR,
									btcol * deltaColG,
									btcol * deltaColB));

								CPoint pol[5];
								CPoint fillpoly[7];
								
								pol[0].x = f1 - helpers::math::round<int,float>(modX*triangle_size_center);
								pol[0].y = f2 - helpers::math::round<int,float>(modY*triangle_size_center);
								pol[1].x = pol[0].x + helpers::math::round<int,float>(modX*triangle_size_tall);
								pol[1].y = pol[0].y + helpers::math::round<int,float>(modY*triangle_size_tall);
								pol[2].x = pol[0].x - helpers::math::round<int,float>(modY*triangle_size_wide);
								pol[2].y = pol[0].y + helpers::math::round<int,float>(modX*triangle_size_wide);
								pol[3].x = pol[0].x + helpers::math::round<int,float>(modX*triangle_size_indent);
								pol[3].y = pol[0].y + helpers::math::round<int,float>(modY*triangle_size_indent);
								pol[4].x = pol[0].x + helpers::math::round<int,float>(modY*triangle_size_wide);
								pol[4].y = pol[0].y - helpers::math::round<int,float>(modX*triangle_size_wide);

								devc->SelectObject(&linepen1);
								amosDraw(devc, oriX, oriY, desX, desY);
								devc->Polygon(&pol[1], 4);
								devc->SelectObject(&linepen2);
								amosDraw(devc, oriX, oriY, desX, desY);
								devc->Polygon(&pol[1], 4);
								devc->SelectObject(&linepen3);
								amosDraw(devc, oriX, oriY, desX, desY);
								devc->Polygon(&pol[1], 4);

								fillpoly[2].x = pol[0].x + helpers::math::round<int,float>(2*modX*triangle_size_indent);
								fillpoly[2].y = pol[0].y + helpers::math::round<int,float>(2*modY*triangle_size_indent);
								fillpoly[6].x = fillpoly[2].x;    
								fillpoly[6].y = fillpoly[2].y;    
								fillpoly[1].x = pol[1].x;         
								fillpoly[1].y = pol[1].y;         
								fillpoly[0].x = pol[2].x;
								fillpoly[0].y = pol[2].y; 
								fillpoly[5].x = pol[2].x;
								fillpoly[5].y = pol[2].y;
								fillpoly[4].x = pol[3].x;
								fillpoly[4].y = pol[3].y;
								fillpoly[3].x = pol[4].x;
								fillpoly[3].y = pol[4].y;

								// fillpoly: (when pointed straight up)
								// top - [1]
								// bottom right corner - [0] and [5]
								// center - [2] and [6] <-- where the three colors meet
								// bottom left corner - [3]
								
								// so the three sides are defined as 0-1-2 (rt), 1-2-3 (lt), and 3-4-5-6 (bt)

								devc->SelectObject(&polyInnardsPen);
								devc->SelectObject(&rtBrush);
								devc->Polygon(fillpoly, 3);
								devc->SelectObject(&ltBrush);
								devc->Polygon(&fillpoly[1], 3);
								devc->SelectObject(&btBrush);
								devc->Polygon(&fillpoly[3], 4);

								devc->SelectObject(GetStockObject(NULL_BRUSH));
								devc->SelectObject(&linepen3);
								devc->Polygon(&pol[1], 4);

								rtBrush.DeleteObject();
								ltBrush.DeleteObject();
								btBrush.DeleteObject();

								tmac->_connectionPoint[w].x = f1-triangle_size_center;
								tmac->_connectionPoint[w].y = f2-triangle_size_center;
							}
						}
					}// Machine actived
				}
				devc->SelectObject(oldpen);
				devc->SelectObject(oldbrush);

				polyInnardsPen.DeleteObject();
				linepen1.DeleteObject();
				linepen2.DeleteObject();
				linepen3.DeleteObject();
			}
			else
			{

				// the shaded arrow colors will be multiplied by these values to convert them from grayscale to the
				// polygon color stated in the config.
				float deltaColR = ((macView->polycolour     & 0xFF) / 510.0) + .45;
				float deltaColG = ((macView->polycolour>>8  & 0xFF) / 510.0) + .45;
				float deltaColB = ((macView->polycolour>>16 & 0xFF) / 510.0) + .45;
				

				// Draw wire [connections]
				
				for(int c=0;c<MAX_MACHINES;c++)
				{
					Machine *tmac=_pSong._pMachine[c];
					if(tmac)
					{

						CPen linepen( PS_SOLID, macView->wirewidth, macView->wirecolour); 
						CPen polyInnardsPen(PS_SOLID, 0, RGB(192 * deltaColR, 192 * deltaColG, 192 * deltaColB));
						CPen *oldpen = devc->SelectObject(&linepen);				
						CBrush *oldbrush = static_cast<CBrush*>(devc->SelectStockObject(NULL_BRUSH));


						int oriX=0;
						int oriY=0;
						switch (tmac->_mode)
						{
						case MACHMODE_GENERATOR:
							oriX = tmac->_x+(MachineCoords->sGenerator.width/2);
							oriY = tmac->_y+(MachineCoords->sGenerator.height/2);
							break;
						case MACHMODE_FX:
							oriX = tmac->_x+(MachineCoords->sEffect.width/2);
							oriY = tmac->_y+(MachineCoords->sEffect.height/2);
							break;

						case MACHMODE_MASTER:
							oriX = tmac->_x+(MachineCoords->sMaster.width/2);
							oriY = tmac->_y+(MachineCoords->sMaster.height/2);
							break;
						}

						for (int w=0; w<MAX_CONNECTIONS; w++)
						{
							if (tmac->outWires[w] && tmac->outWires[w]->Enabled())
							{
								int desX = 0;
								int desY = 0;
								Machine& pout = tmac->outWires[w]->GetDstMachine();
								switch (pout._mode)
								{
								case MACHMODE_GENERATOR:
									desX = pout._x+(MachineCoords->sGenerator.width/2);
									desY = pout._y+(MachineCoords->sGenerator.height/2);
									break;
								case MACHMODE_FX:
									desX = pout._x+(MachineCoords->sEffect.width/2);
									desY = pout._y+(MachineCoords->sEffect.height/2);
									break;

								case MACHMODE_MASTER:
									desX = pout._x+(MachineCoords->sMaster.width/2);
									desY = pout._y+(MachineCoords->sMaster.height/2);
									break;
								}
								
								int const f1 = (desX+oriX)/2;
								int const f2 = (desY+oriY)/2;

								amosDraw(devc, oriX, oriY, desX, desY);
								
								double modX = double(desX-oriX);
								double modY = double(desY-oriY);
								double modT = sqrt(modX*modX+modY*modY);
								
								modX = modX/modT;
								modY = modY/modT;
								double slope = atan2(modY, modX);
								double altslope;
										
								int rtcol = 140+abs(helpers::math::round<int,float>(slope*32));

								altslope=slope;
								if(altslope<-1.05)  altslope -= 2 * (altslope + 1.05);
								if(altslope>2.10) altslope -= 2 * (altslope - 2.10);
								int ltcol = 140 + abs(helpers::math::round<int,float>((altslope - 2.10) * 32));

								altslope=slope;
								if(altslope>0.79)  altslope -= 2 * (altslope - 0.79);
								if(altslope<-2.36)  altslope -= 2 * (altslope + 2.36);
								int btcol = 240 - abs(helpers::math::round<int,float>((altslope-0.79) * 32));

								// brushes for the right side, left side, and bottom of the arrow (when pointed straight up).
								CBrush rtBrush(rgb(
									rtcol * deltaColR,
									rtcol * deltaColG,
									rtcol * deltaColB));
								CBrush ltBrush(rgb(
									ltcol * deltaColR,
									ltcol * deltaColG,
									ltcol * deltaColB));
								CBrush btBrush(rgb(
									btcol * deltaColR,
									btcol * deltaColG,
									btcol * deltaColB));

								CPoint pol[5];
								CPoint fillpoly[7];
								
								pol[0].x = f1 - helpers::math::round<int,float>(modX*triangle_size_center);
								pol[0].y = f2 - helpers::math::round<int,float>(modY*triangle_size_center);
								pol[1].x = pol[0].x + helpers::math::round<int,float>(modX*triangle_size_tall);
								pol[1].y = pol[0].y + helpers::math::round<int,float>(modY*triangle_size_tall);
								pol[2].x = pol[0].x - helpers::math::round<int,float>(modY*triangle_size_wide);
								pol[2].y = pol[0].y + helpers::math::round<int,float>(modX*triangle_size_wide);
								pol[3].x = pol[0].x + helpers::math::round<int,float>(modX*triangle_size_indent);
								pol[3].y = pol[0].y + helpers::math::round<int,float>(modY*triangle_size_indent);
								pol[4].x = pol[0].x + helpers::math::round<int,float>(modY*triangle_size_wide);
								pol[4].y = pol[0].y - helpers::math::round<int,float>(modX*triangle_size_wide);										

								devc->Polygon(&pol[1], 4);

								fillpoly[2].x = pol[0].x + helpers::math::round<int,float>(2*modX*triangle_size_indent);
								fillpoly[2].y = pol[0].y + helpers::math::round<int,float>(2*modY*triangle_size_indent);
								fillpoly[6].x = fillpoly[2].x;
								fillpoly[6].y = fillpoly[2].y;
								fillpoly[1].x = pol[1].x;
								fillpoly[1].y = pol[1].y;
								fillpoly[0].x = pol[2].x;
								fillpoly[0].y = pol[2].y;
								fillpoly[5].x = pol[2].x;
								fillpoly[5].y = pol[2].y;
								fillpoly[4].x = pol[3].x;
								fillpoly[4].y = pol[3].y;
								fillpoly[3].x = pol[4].x;
								fillpoly[3].y = pol[4].y;

								// fillpoly: (when pointed straight up)
								// top - [1]
								// bottom right corner - [0] and [5]
								// center - [2] and [6] <-- where the three colors meet
								// bottom left corner - [3]
								
								// so the three sides are defined as 0-1-2 (rt), 1-2-3 (lt), and 3-4-5-6 (bt)

								devc->SelectObject(&polyInnardsPen);
								devc->SelectObject(&rtBrush);
								devc->Polygon(fillpoly, 3);
								devc->SelectObject(&ltBrush);
								devc->Polygon(&fillpoly[1], 3);
								devc->SelectObject(&btBrush);
								devc->Polygon(&fillpoly[3], 4);

								devc->SelectObject(GetStockObject(NULL_BRUSH));
								devc->SelectObject(&linepen);
								devc->Polygon(&pol[1], 4);

								rtBrush.DeleteObject();
								ltBrush.DeleteObject();
								btBrush.DeleteObject();

								tmac->_connectionPoint[w].x = f1-triangle_size_center;
								tmac->_connectionPoint[w].y = f2-triangle_size_center;
							}
						}
				devc->SelectObject(oldpen);
						devc->SelectObject(oldbrush);
						polyInnardsPen.DeleteObject();
	
				linepen.DeleteObject();
					
					}// Machine actived
				}
			}


			// Draw machine boxes
			for (int c=0; c<MAX_MACHINES; c++)
			{
				if(_pSong._pMachine[c])
				{
					DrawMachine(c , devc);
				}// Machine exist
			}

			// draw vumeters
			DrawAllMachineVumeters((CClientDC*)devc);

			if ((wiresource != -1) || (wiredest != -1))
			{
				int prevROP = devc->SetROP2(R2_NOT);
				amosDraw(devc, wireSX, wireSY, wireDX, wireDY);
				devc->SetROP2(prevROP);
			}
			devc->SelectObject(oldbrush);
			fillbrush.DeleteObject();
		}

		//////////////////////////////////////////////////////////////////////
		// Draws a single machine

		void CChildView::DrawMachineVol(int c,CDC *devc)
		{
			Machine* pMac = Global::song()._pMachine[c];
			if (pMac)
			{
				CDC memDC;
				CBitmap* oldbmp;
				memDC.CreateCompatibleDC(devc);
				oldbmp=memDC.SelectObject(&macView->machineskin);

				int vol = pMac->_volumeDisplay;
				int max = pMac->_volumeMaxDisplay;

				switch (pMac->_mode)
				{
				case MACHMODE_GENERATOR:
					// scale our volumes
					vol *= MachineCoords->dGeneratorVu.width;
					vol /= 96;

					max *= MachineCoords->dGeneratorVu.width;
					max /= 96;

					if (MachineCoords->bHasTransparency)
					{
						// BLIT [DESTX,DESTY,SIZEX,SIZEY,source,BMPX,BMPY,mode]
						if (vol > 0)
						{
							if (MachineCoords->sGeneratorVu0.width)
							{
								vol /= MachineCoords->sGeneratorVu0.width;// restrict to leds
								vol *= MachineCoords->sGeneratorVu0.width;
							}
						}
						else
						{
							vol = 0;
						}

						RECT r;
						r.left = pMac->_x+vol+MachineCoords->dGeneratorVu.x;
						r.top = pMac->_y+MachineCoords->dGeneratorVu.y;
						r.right = r.left + MachineCoords->dGeneratorVu.width-vol;
						r.bottom = r.top + MachineCoords->sGeneratorVu0.height;
						devc->FillSolidRect(&r,macView->colour);

						TransparentBlt(devc,
									r.left, 
									r.top, 
									MachineCoords->dGeneratorVu.width-vol, 
									MachineCoords->sGeneratorVu0.height, 
									&memDC, 
									&macView->machineskinmask,
									MachineCoords->sGenerator.x+MachineCoords->dGeneratorVu.x+vol, 
									MachineCoords->sGenerator.y+MachineCoords->dGeneratorVu.y);

						if (max > 0)
						{
							if (MachineCoords->sGeneratorVuPeak.width)
							{
								max /= MachineCoords->sGeneratorVuPeak.width;// restrict to leds
								max *= MachineCoords->sGeneratorVuPeak.width;
								TransparentBlt(devc,
											pMac->_x+max+MachineCoords->dGeneratorVu.x, 
											pMac->_y+MachineCoords->dGeneratorVu.y, 
											MachineCoords->sGeneratorVuPeak.width, 
											MachineCoords->sGeneratorVuPeak.height, 
											&memDC, 
											&macView->machineskinmask,
											MachineCoords->sGeneratorVuPeak.x, 
											MachineCoords->sGeneratorVuPeak.y);
							}
						}

						if (vol > 0)
						{
							TransparentBlt(devc,
										pMac->_x+MachineCoords->dGeneratorVu.x, 
										pMac->_y+MachineCoords->dGeneratorVu.y, 
										vol, 
										MachineCoords->sGeneratorVu0.height, 
										&memDC, 
										&macView->machineskinmask,
										MachineCoords->sGeneratorVu0.x, 
										MachineCoords->sGeneratorVu0.y);
						}
					}
					else
					{
						// BLIT [DESTX,DESTY,SIZEX,SIZEY,source,BMPX,BMPY,mode]
						if (vol > 0)
						{
							if (MachineCoords->sGeneratorVu0.width)
							{
								vol /= MachineCoords->sGeneratorVu0.width;// restrict to leds
								vol *= MachineCoords->sGeneratorVu0.width;
							}
						}
						else
						{
							vol = 0;
						}

						devc->BitBlt(pMac->_x+vol+MachineCoords->dGeneratorVu.x, 
										pMac->_y+MachineCoords->dGeneratorVu.y, 
										MachineCoords->dGeneratorVu.width-vol, 
										MachineCoords->sGeneratorVu0.height, 
										&memDC, 
										MachineCoords->sGenerator.x+MachineCoords->dGeneratorVu.x+vol, 
										MachineCoords->sGenerator.y+MachineCoords->dGeneratorVu.y, 
										SRCCOPY); //background

						if (max > 0)
						{
							if (MachineCoords->sGeneratorVuPeak.width)
							{
								max /= MachineCoords->sGeneratorVuPeak.width;// restrict to leds
								max *= MachineCoords->sGeneratorVuPeak.width;
								devc->BitBlt(pMac->_x+max+MachineCoords->dGeneratorVu.x, 
											pMac->_y+MachineCoords->dGeneratorVu.y, 
											MachineCoords->sGeneratorVuPeak.width, 
											MachineCoords->sGeneratorVuPeak.height, 
											&memDC, 
											MachineCoords->sGeneratorVuPeak.x, 
											MachineCoords->sGeneratorVuPeak.y, 
											SRCCOPY); //peak
							}
						}

						if (vol > 0)
						{
							devc->BitBlt(pMac->_x+MachineCoords->dGeneratorVu.x, 
										pMac->_y+MachineCoords->dGeneratorVu.y, 
										vol, 
										MachineCoords->sGeneratorVu0.height, 
										&memDC, 
										MachineCoords->sGeneratorVu0.x, 
										MachineCoords->sGeneratorVu0.y, 
										SRCCOPY); // leds
						}
					}

					break;
				case MACHMODE_FX:
					// scale our volumes
					vol *= MachineCoords->dEffectVu.width;
					vol /= 96;

					max *= MachineCoords->dEffectVu.width;
					max /= 96;

					if (MachineCoords->bHasTransparency)
					{
						// BLIT [DESTX,DESTY,SIZEX,SIZEY,source,BMPX,BMPY,mode]
						if (vol > 0)
						{
							if (MachineCoords->sEffectVu0.width)
							{
								vol /= MachineCoords->sEffectVu0.width;// restrict to leds
								vol *= MachineCoords->sEffectVu0.width;
							}
						}
						else
						{
							vol = 0;
						}

						RECT r;
						r.left = pMac->_x+vol+MachineCoords->dEffectVu.x;
						r.top = pMac->_y+MachineCoords->dEffectVu.y;
						r.right = r.left + MachineCoords->dEffectVu.width-vol;
						r.bottom = r.top + MachineCoords->sEffectVu0.height;
						devc->FillSolidRect(&r,macView->colour);

						TransparentBlt(devc,
									r.left, 
									r.top, 
									MachineCoords->dEffectVu.width-vol, 
									MachineCoords->sEffectVu0.height, 
									&memDC, 
									&macView->machineskinmask,
									MachineCoords->sEffect.x+MachineCoords->dEffectVu.x+vol, 
									MachineCoords->sEffect.y+MachineCoords->dEffectVu.y);

						if (max > 0)
						{
							if (MachineCoords->sEffectVuPeak.width)
							{
								max /= MachineCoords->sEffectVuPeak.width;// restrict to leds
								max *= MachineCoords->sEffectVuPeak.width;
								TransparentBlt(devc,
											pMac->_x+max+MachineCoords->dEffectVu.x, 
											pMac->_y+MachineCoords->dEffectVu.y, 
											MachineCoords->sEffectVuPeak.width, 
											MachineCoords->sEffectVuPeak.height, 
											&memDC, 
											&macView->machineskinmask,
											MachineCoords->sEffectVuPeak.x, 
											MachineCoords->sEffectVuPeak.y);
							}
						}

						if (vol > 0)
						{
							TransparentBlt(devc,
										pMac->_x+MachineCoords->dEffectVu.x, 
										pMac->_y+MachineCoords->dEffectVu.y, 
										vol, 
										MachineCoords->sEffectVu0.height, 
										&memDC, 
										&macView->machineskinmask,
										MachineCoords->sEffectVu0.x, 
										MachineCoords->sEffectVu0.y);
						}
					}
					else
					{
						// BLIT [DESTX,DESTY,SIZEX,SIZEY,source,BMPX,BMPY,mode]
						if (vol > 0)
						{
							if (MachineCoords->sEffectVu0.width)
							{
								vol /= MachineCoords->sEffectVu0.width;// restrict to leds
								vol *= MachineCoords->sEffectVu0.width;
							}
						}
						else
						{
							vol = 0;
						}

						devc->BitBlt(pMac->_x+vol+MachineCoords->dEffectVu.x, 
										pMac->_y+MachineCoords->dEffectVu.y, 
										MachineCoords->dEffectVu.width-vol, 
										MachineCoords->sEffectVu0.height, 
										&memDC, 
										MachineCoords->sEffect.x+MachineCoords->dEffectVu.x+vol, 
										MachineCoords->sEffect.y+MachineCoords->dEffectVu.y, 
										SRCCOPY); //background

						if (max > 0)
						{
							if (MachineCoords->sEffectVuPeak.width)
							{
								max /= MachineCoords->sEffectVuPeak.width;// restrict to leds
								max *= MachineCoords->sEffectVuPeak.width;
								devc->BitBlt(pMac->_x+max+MachineCoords->dEffectVu.x, 
											pMac->_y+MachineCoords->dEffectVu.y, 
											MachineCoords->sEffectVuPeak.width, 
											MachineCoords->sEffectVuPeak.height, 
											&memDC, 
											MachineCoords->sEffectVuPeak.x, 
											MachineCoords->sEffectVuPeak.y, 
											SRCCOPY); //peak
							}
						}

						if (vol > 0)
						{
							devc->BitBlt(pMac->_x+MachineCoords->dEffectVu.x, 
										pMac->_y+MachineCoords->dEffectVu.y, 
										vol, 
										MachineCoords->sEffectVu0.height, 
										&memDC, 
										MachineCoords->sEffectVu0.x, 
										MachineCoords->sEffectVu0.y, 
										SRCCOPY); // leds
						}
					}

					break;

				}

				memDC.SelectObject(oldbmp);
			}
		}

		void CChildView::ClearMachineSpace(int macnum, CDC *devc)
		{
			Machine* mac = _pSong._pMachine[macnum];
			if(!mac)
			{
				return;
			}

			if (MachineCoords->bHasTransparency)
			{
				RECT r;
				switch (mac->_mode)
				{
				case MACHMODE_GENERATOR:
					r.left = mac->_x;
					r.top = mac->_y;
					r.right = r.left + MachineCoords->sGenerator.width;
					r.bottom = r.top + MachineCoords->sGenerator.height;
					devc->FillSolidRect(&r,macView->colour);
					break;
				case MACHMODE_FX:
					r.left = mac->_x;
					r.top = mac->_y;
					r.right = r.left + MachineCoords->sEffect.width;
					r.bottom = r.top + MachineCoords->sEffect.height;
					devc->FillSolidRect(&r,macView->colour);
					break;
				}
			}
		}

		void CChildView::DrawMachineHighlight(int macnum, CDC *devc, Machine *mac, int x, int y)
		{
			//the code below draws the highlight around the selected machine (the corners)

			CPoint pol[3];
			CPen linepen( PS_SOLID, macView->wirewidth, macView->wirecolour); 
			CPen *oldpen = devc->SelectObject(&linepen);

			int inst=-1;
			int selbus;
			Machine * selmac = _pSong.GetMachineOfBus(_pSong.seqBus, inst);
			if ( selmac != NULL ) { selbus = selmac->_macIndex; }
			else { selbus = _pSong.seqBus; }

			int hlength = 9; //the length of the selected machine highlight
			int hdistance = 5; //the distance of the highlight from the machine

			switch (mac->_mode) 
			{
			case MACHMODE_GENERATOR:
				if (macnum == selbus) {	

					pol[0].x = x - hdistance;
					pol[0].y = y - hdistance + hlength;
					pol[1].x = x - hdistance;
					pol[1].y = y - hdistance;
					pol[2].x = x - hdistance + hlength;
					pol[2].y = y - hdistance;

					devc->Polyline(&pol[0], 3);

					pol[0].x = x + MachineCoords->sGenerator.width + hdistance - hlength;
					pol[0].y = y - hdistance;
					pol[1].x = x + MachineCoords->sGenerator.width + hdistance;
					pol[1].y = y - hdistance;
					pol[2].x = x + MachineCoords->sGenerator.width + hdistance;
					pol[2].y = y - hdistance + hlength;

					devc->Polyline(&pol[0], 3);

					pol[0].x = x + MachineCoords->sGenerator.width + hdistance;
					pol[0].y = y + MachineCoords->sGenerator.height + hdistance - hlength;
					pol[1].x = x + MachineCoords->sGenerator.width + hdistance;
					pol[1].y = y + MachineCoords->sGenerator.height + hdistance;
					pol[2].x = x + MachineCoords->sGenerator.width + hdistance - hlength;
					pol[2].y = y + MachineCoords->sGenerator.height + hdistance;

					devc->Polyline(&pol[0], 3);

					pol[0].x = x - hdistance + hlength;
					pol[0].y = y + MachineCoords->sGenerator.height + hdistance;
					pol[1].x = x - hdistance;
					pol[1].y = y + MachineCoords->sGenerator.height + hdistance;
					pol[2].x = x - hdistance;
					pol[2].y = y + MachineCoords->sGenerator.height + hdistance - hlength;

					devc->Polyline(&pol[0], 3);

				}
				break;
			case MACHMODE_FX:
				if (macnum == selbus) {

					pol[0].x = x - hdistance;
					pol[0].y = y - hdistance + hlength;
					pol[1].x = x - hdistance;
					pol[1].y = y - hdistance;
					pol[2].x = x - hdistance + hlength;
					pol[2].y = y - hdistance;

					devc->Polyline(&pol[0], 3);

					pol[0].x = x + MachineCoords->sEffect.width + hdistance - hlength;
					pol[0].y = y - hdistance;
					pol[1].x = x + MachineCoords->sEffect.width + hdistance;
					pol[1].y = y - hdistance;
					pol[2].x = x + MachineCoords->sEffect.width + hdistance;
					pol[2].y = y - hdistance + hlength;

					devc->Polyline(&pol[0], 3);

					pol[0].x = x + MachineCoords->sEffect.width + hdistance;
					pol[0].y = y + MachineCoords->sEffect.height + hdistance - hlength;
					pol[1].x = x + MachineCoords->sEffect.width + hdistance;
					pol[1].y = y + MachineCoords->sEffect.height + hdistance;
					pol[2].x = x + MachineCoords->sEffect.width + hdistance - hlength;
					pol[2].y = y + MachineCoords->sEffect.height + hdistance;

					devc->Polyline(&pol[0], 3);

					pol[0].x = x - hdistance + hlength;
					pol[0].y = y + MachineCoords->sEffect.height + hdistance;
					pol[1].x = x - hdistance;
					pol[1].y = y + MachineCoords->sEffect.height + hdistance;
					pol[2].x = x - hdistance;
					pol[2].y = y + MachineCoords->sEffect.height + hdistance - hlength;

					devc->Polyline(&pol[0], 3);
				}
				break;
			}
			devc->SelectObject(oldpen);

			//end of highlighting code
		}

		void CChildView::DrawMachine(int macnum, CDC *devc)
		{
			Machine* mac = _pSong._pMachine[macnum];
			if(!mac)
			{
				return;
			}

			int x=mac->_x;
			int y=mac->_y;

			CDC memDC;
			memDC.CreateCompatibleDC(devc);
			CBitmap* oldbmp = memDC.SelectObject(&macView->machineskin);

			DrawMachineHighlight(macnum, devc, mac, x, y);

			// BLIT [DESTX,DESTY,SIZEX,SIZEY,source,BMPX,BMPY,mode]
			if (MachineCoords->bHasTransparency)
			{
				switch (mac->_mode)
				{
				case MACHMODE_GENERATOR:
					/*
					RECT r;
					r.left = x;
					r.top = y;
					r.right = r.left + MachineCoords->sGenerator.width;
					r.bottom = r.top + MachineCoords->sGenerator.height;
					devc->FillSolidRect(&r,macView->colour);
					*/

					TransparentBlt(devc,
								x, 
								y, 
								MachineCoords->sGenerator.width, 
								MachineCoords->sGenerator.height, 
								&memDC, 
								&macView->machineskinmask,
								MachineCoords->sGenerator.x, 
								MachineCoords->sGenerator.y);
					// Draw pan
					{
						int panning = mac->GetPan()*MachineCoords->dGeneratorPan.width;
						panning /= 128;
						TransparentBlt(devc,
									x+panning+MachineCoords->dGeneratorPan.x, 
									y+MachineCoords->dGeneratorPan.y, 
									MachineCoords->sGeneratorPan.width, 
									MachineCoords->sGeneratorPan.height, 
									&memDC, 
									&macView->machineskinmask,
									MachineCoords->sGeneratorPan.x, 
									MachineCoords->sGeneratorPan.y);
					}
					if (mac->_mute)
					{
						TransparentBlt(devc,
									x+MachineCoords->dGeneratorMute.x, 
									y+MachineCoords->dGeneratorMute.y, 
									MachineCoords->sGeneratorMute.width, 
									MachineCoords->sGeneratorMute.height, 
									&memDC, 
									&macView->machineskinmask,
									MachineCoords->sGeneratorMute.x, 
									MachineCoords->sGeneratorMute.y);
					}
					else if (_pSong.machineSoloed == macnum)
					{
						TransparentBlt(devc,
									x+MachineCoords->dGeneratorSolo.x, 
									y+MachineCoords->dGeneratorSolo.y, 
									MachineCoords->sGeneratorSolo.width, 
									MachineCoords->sGeneratorSolo.height, 
									&memDC, 
									&macView->machineskinmask,
									MachineCoords->sGeneratorSolo.x, 
									MachineCoords->sGeneratorSolo.y);
					}
					// Draw text
					{
						CFont* oldFont= devc->SelectObject(&macView->generatorFont);
						devc->SetBkMode(TRANSPARENT);
						devc->SetTextColor(macView->generator_fontcolour);
						char name[sizeof(mac->_editName)+6+3];
						int boxx=x+MachineCoords->dGeneratorName.x;
						int boxy=y+MachineCoords->dGeneratorName.y;
						if (macView->draw_mac_index) {
							sprintf(name,"%.2X:%s",mac->_macIndex,mac->_editName);
						} else { sprintf(name,"%s",mac->_macIndex,mac->_editName); }
						if (MachineCoords->dGeneratorNameClip.x > 0 ) {
							devc->ExtTextOut(boxx, boxy, ETO_CLIPPED , 
								CRect(boxx,boxy,boxx+MachineCoords->dGeneratorNameClip.x, boxy+MachineCoords->dGeneratorNameClip.y), 
								name, 0);
						}
						else {
							devc->TextOut(boxx, boxy, name);
						}
						devc->SetBkMode(OPAQUE);
						devc->SelectObject(oldFont);
					}
					break;
				case MACHMODE_FX:
					/*
					RECT r;
					r.left = x;
					r.top = y;
					r.right = r.left + MachineCoords->sEffect.width;
					r.bottom = r.top + MachineCoords->sEffect.height;
					devc->FillSolidRect(&r,macView->colour);
					*/

					TransparentBlt(devc,
								x, 
								y,
								MachineCoords->sEffect.width, 
								MachineCoords->sEffect.height, 
								&memDC, 
								&macView->machineskinmask,
								MachineCoords->sEffect.x, 
								MachineCoords->sEffect.y);
					// Draw pan
					{
						int panning = mac->GetPan()*MachineCoords->dEffectPan.width;
						panning /= 128;
						TransparentBlt(devc,
									x+panning+MachineCoords->dEffectPan.x, 
									y+MachineCoords->dEffectPan.y, 
									MachineCoords->sEffectPan.width, 
									MachineCoords->sEffectPan.height, 
									&memDC, 
									&macView->machineskinmask,
									MachineCoords->sEffectPan.x, 
									MachineCoords->sEffectPan.y);
					}
					if (mac->_mute)
					{
						TransparentBlt(devc,
									x+MachineCoords->dEffectMute.x, 
									y+MachineCoords->dEffectMute.y, 
									MachineCoords->sEffectMute.width, 
									MachineCoords->sEffectMute.height, 
									&memDC, 
									&macView->machineskinmask,
									MachineCoords->sEffectMute.x, 
									MachineCoords->sEffectMute.y);
					}
					if (mac->Bypass())
					{
						TransparentBlt(devc,
									x+MachineCoords->dEffectBypass.x, 
									y+MachineCoords->dEffectBypass.y, 
									MachineCoords->sEffectBypass.width, 
									MachineCoords->sEffectBypass.height, 
									&memDC, 
									&macView->machineskinmask,
									MachineCoords->sEffectBypass.x, 
									MachineCoords->sEffectBypass.y);
					}
					// Draw text
					{
						CFont* oldFont= devc->SelectObject(&macView->effectFont);
						devc->SetBkMode(TRANSPARENT);
						devc->SetTextColor(macView->effect_fontcolour);
						char name[sizeof(mac->_editName)+6+3];
						int boxx=x+MachineCoords->dEffectName.x;
						int boxy=y+MachineCoords->dEffectName.y;
						if (macView->draw_mac_index) {
							sprintf(name,"%.2X:%s",mac->_macIndex,mac->_editName);
						} else { sprintf(name,"%s",mac->_macIndex,mac->_editName); }
						if (MachineCoords->dEffectNameClip.x > 0 ) {
							devc->ExtTextOut(boxx, boxy, ETO_CLIPPED , 
								CRect(boxx,boxy,boxx+MachineCoords->dEffectNameClip.x, boxy+MachineCoords->dEffectNameClip.y), 
								name, 0);
						}
						else {
							devc->TextOut(boxx, boxy, name);
						}
						devc->SetBkMode(OPAQUE);
						devc->SelectObject(oldFont);
					}
					break;

				case MACHMODE_MASTER:
					TransparentBlt(devc,
								x, 
								y,
								MachineCoords->sMaster.width, 
								MachineCoords->sMaster.height, 
								&memDC, 
								&macView->machineskinmask,
								MachineCoords->sMaster.x, 
								MachineCoords->sMaster.y);
					break;
				}
			}
			else
			{
				switch (mac->_mode)
				{
				case MACHMODE_GENERATOR:
					devc->BitBlt(x, 
								y, 
								MachineCoords->sGenerator.width, 
								MachineCoords->sGenerator.height, 
								&memDC, 
								MachineCoords->sGenerator.x, 
								MachineCoords->sGenerator.y, 
								SRCCOPY);
					// Draw pan
					{
						int panning = mac->GetPan()*MachineCoords->dGeneratorPan.width;
						panning /= 128;
						devc->BitBlt(x+panning+MachineCoords->dGeneratorPan.x, 
									y+MachineCoords->dGeneratorPan.y, 
									MachineCoords->sGeneratorPan.width, 
									MachineCoords->sGeneratorPan.height, 
									&memDC, 
									MachineCoords->sGeneratorPan.x, 
									MachineCoords->sGeneratorPan.y, 
									SRCCOPY);
					}
					if (mac->_mute)
					{
						devc->BitBlt(x+MachineCoords->dGeneratorMute.x, 
									y+MachineCoords->dGeneratorMute.y, 
									MachineCoords->sGeneratorMute.width, 
									MachineCoords->sGeneratorMute.height, 
									&memDC, 
									MachineCoords->sGeneratorMute.x, 
									MachineCoords->sGeneratorMute.y, 
									SRCCOPY);
					}
					else if (_pSong.machineSoloed == macnum)
					{
						devc->BitBlt(x+MachineCoords->dGeneratorSolo.x, 
									y+MachineCoords->dGeneratorSolo.y, 
									MachineCoords->sGeneratorSolo.width, 
									MachineCoords->sGeneratorSolo.height, 
									&memDC, 
									MachineCoords->sGeneratorSolo.x, 
									MachineCoords->sGeneratorSolo.y, 
									SRCCOPY);
					}
					// Draw text
					{
						CFont* oldFont= devc->SelectObject(&macView->generatorFont);
						devc->SetBkMode(TRANSPARENT);
						devc->SetTextColor(macView->generator_fontcolour);
						char name[sizeof(mac->_editName)+6+3];
						int boxx=x+MachineCoords->dGeneratorName.x;
						int boxy=y+MachineCoords->dGeneratorName.y;
						if (macView->draw_mac_index) {
							sprintf(name,"%.2X:%s",mac->_macIndex,mac->_editName);
						} else { sprintf(name,"%s",mac->_macIndex,mac->_editName); }
						if (MachineCoords->dGeneratorNameClip.x > 0 ) {
							devc->ExtTextOut(boxx, boxy, ETO_CLIPPED , 
								CRect(boxx,boxy,boxx+MachineCoords->dGeneratorNameClip.x, boxy+MachineCoords->dGeneratorNameClip.y), 
								name, 0);
						}
						else {
							devc->TextOut(boxx, boxy, name);
						}
						devc->SetBkMode(OPAQUE);
						devc->SelectObject(oldFont);
					}
					break;
				case MACHMODE_FX:
					devc->BitBlt(x, 
								y,
								MachineCoords->sEffect.width, 
								MachineCoords->sEffect.height, 
								&memDC, 
								MachineCoords->sEffect.x, 
								MachineCoords->sEffect.y, 
								SRCCOPY);
					// Draw pan
					{
						int panning = mac->GetPan()*MachineCoords->dEffectPan.width;
						panning /= 128;
						devc->BitBlt(x+panning+MachineCoords->dEffectPan.x, 
									y+MachineCoords->dEffectPan.y, 
									MachineCoords->sEffectPan.width, 
									MachineCoords->sEffectPan.height, 
									&memDC, 
									MachineCoords->sEffectPan.x, 
									MachineCoords->sEffectPan.y, 
									SRCCOPY);
					}
					if (mac->_mute)
					{
						devc->BitBlt(x+MachineCoords->dEffectMute.x, 
									y+MachineCoords->dEffectMute.y, 
									MachineCoords->sEffectMute.width, 
									MachineCoords->sEffectMute.height, 
									&memDC, 
									MachineCoords->sEffectMute.x, 
									MachineCoords->sEffectMute.y, 
									SRCCOPY);
					}
					if (mac->Bypass())
					{
						devc->BitBlt(x+MachineCoords->dEffectBypass.x, 
									y+MachineCoords->dEffectBypass.y, 
									MachineCoords->sEffectBypass.width, 
									MachineCoords->sEffectBypass.height, 
									&memDC, 
									MachineCoords->sEffectBypass.x, 
									MachineCoords->sEffectBypass.y, 
									SRCCOPY);
					}
					// Draw text
					{
						CFont* oldFont= devc->SelectObject(&macView->effectFont);
						devc->SetBkMode(TRANSPARENT);
						devc->SetTextColor(macView->effect_fontcolour);
						char name[sizeof(mac->_editName)+6+3];
						int boxx=x+MachineCoords->dEffectName.x;
						int boxy=y+MachineCoords->dEffectName.y;
						if (macView->draw_mac_index) {
							sprintf(name,"%.2X:%s",mac->_macIndex,mac->_editName);
						} else { sprintf(name,"%s",mac->_macIndex,mac->_editName); }
						if (MachineCoords->dEffectNameClip.x > 0 ) {
							devc->ExtTextOut(boxx, boxy, ETO_CLIPPED , 
								CRect(boxx,boxy,boxx+MachineCoords->dEffectNameClip.x, boxy+MachineCoords->dEffectNameClip.y), 
								name, 0);
						}
						else {
							devc->TextOut(boxx, boxy, name);
						}
						devc->SetBkMode(OPAQUE);
						devc->SelectObject(oldFont);
					}
					break;

				case MACHMODE_MASTER:
					devc->BitBlt(x, 
								y,
								MachineCoords->sMaster.width, 
								MachineCoords->sMaster.height, 
								&memDC, 
								MachineCoords->sMaster.x, 
								MachineCoords->sMaster.y, 
								SRCCOPY);
					break;
				}
			}
			memDC.SelectObject(oldbmp);
			memDC.DeleteDC();
			
		}


		void CChildView::amosDraw(CDC *devc, int oX,int oY,int dX,int dY)
		{
			if (oX == dX)
			{
				oX++;
			}
			if (oY == dY)
			{
				oY++;
			}

			devc->MoveTo(oX,oY);
			devc->LineTo(dX,dY);	
		}

		int CChildView::GetMachine(CPoint point)
		{
			int tmac = -1;
			
			for (int c=MAX_MACHINES-1; c>=0; c--)
			{
				Machine* pMac = Global::song()._pMachine[c];
				if (pMac)
				{
					int x1 = pMac->_x;
					int y1 = pMac->_y;
					int x2=0,y2=0;
					switch (pMac->_mode)
					{
					case MACHMODE_GENERATOR:
						x2 = pMac->_x+MachineCoords->sGenerator.width;
						y2 = pMac->_y+MachineCoords->sGenerator.height;
						break;
					case MACHMODE_FX:
						x2 = pMac->_x+MachineCoords->sEffect.width;
						y2 = pMac->_y+MachineCoords->sEffect.height;
						break;

					case MACHMODE_MASTER:
						x2 = pMac->_x+MachineCoords->sMaster.width;
						y2 = pMac->_y+MachineCoords->sMaster.height;
						break;
					}
					
					if (point.x > x1 && point.x < x2 && point.y > y1 && point.y < y2)
					{
						tmac = c;
						break;
					}
				}
			}
			return tmac;
		}
		Wire* CChildView::GetWire(CPoint point, int& srcOutIdx, int& destInIdx)
		{
			for (int c=0; c<MAX_MACHINES; c++)
			{
				Machine *tmac = Global::song()._pMachine[c];
				if (tmac)
				{
					for (int w = 0; w<MAX_CONNECTIONS; w++)
					{
						if (tmac->outWires[w] && tmac->outWires[w]->Enabled())
						{
							int xt = tmac->_connectionPoint[w].x;
							int yt = tmac->_connectionPoint[w].y;

							if ((point.x > xt) && (point.x < xt+triangle_size_tall) && (point.y > yt) && (point.y < yt+triangle_size_tall))
							{
								srcOutIdx = w;
								destInIdx = tmac->outWires[w]->GetDstWireIndex();
								return tmac->outWires[w];
							}
						}
					}
				}
			}
			srcOutIdx=-1;
			destInIdx=-1;
			return NULL;
		}

}}
