/*
 * Copyright (C)2019 Roger Clark. VK3KYY / G4KYF
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The name of the author may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <menu/menuSystem.h>
#include "menu/menuUtilityQSOData.h"
#include "fw_trx.h"
#include "fw_EEPROM.h"
#include "fw_SPI_Flash.h"

void updateLastHeardList(int id,int talkGroup);

static const int DMRID_MEMORY_STORAGE_START = 0x30000;
static const int DMRID_HEADER_LENGTH = 0x0C;
LinkItem_t callsList[NUM_LASTHEARD_STORED];
LinkItem_t *LinkHead = callsList;
int numLastHeard=0;
int menuDisplayQSODataState = QSO_DISPLAY_DEFAULT_SCREEN;

void lastheardInitList()
{
    LinkHead = callsList;

    for(int i=0;i<NUM_LASTHEARD_STORED;i++)
    {
    	callsList[i].id=0;
        callsList[i].talkGroup=0;
        if (i==0)
        {
            callsList[i].prev=NULL;
        }
        else
        {
            callsList[i].prev=&callsList[i-1];
        }
        if (i<(NUM_LASTHEARD_STORED-1))
        {
            callsList[i].next=&callsList[i+1];
        }
        else
        {
            callsList[i].next=NULL;
        }
    }
/* TEST DATA
    updateLastHeardList(5053344,505);
    updateLastHeardList(5054045,505);
    updateLastHeardList(5054125,505);
    updateLastHeardList(5053238,505);
    updateLastHeardList(5053007,505);
    updateLastHeardList(5053407,505);
    updateLastHeardList(5054223,505);
    updateLastHeardList(1234567,505);
*/
}

LinkItem_t * findInList(int id)
{
    LinkItem_t *item = LinkHead;

    while(item->next!=NULL)
    {
        if (item->id==id)
        {
            // found it
            return item;
        }
        item=item->next;
    }
    return NULL;
}

int lastID=0;

void lastHeardListUpdate(uint8_t *dmrDataBuffer)
{
	if (dmrDataBuffer[0] == 0x00)
	{
		int talkGroup=(dmrDataBuffer[3]<<16)+(dmrDataBuffer[4]<<8)+(dmrDataBuffer[5]<<0);
		int id=(dmrDataBuffer[6]<<16)+(dmrDataBuffer[7]<<8)+(dmrDataBuffer[8]<<0);

		if (id!=lastID)
		{
			lastID=id;
			menuDisplayQSODataState = QSO_DISPLAY_CALLER_DATA;// flag that the display needs to update
			LinkItem_t *item = findInList(id);

			if (item!=NULL)
			{
				// Already in the list


				item->talkGroup = talkGroup;// update the TG in case they changed TG

				if (item == LinkHead)
				{

					return;// already at top of the list
				}
				else
				{
					// not at top of the list
					// Move this item to the top of the list
					LinkItem_t *next=item->next;
					LinkItem_t *prev=item->prev;

					// set the previous item to skip this item and link to 'items' next item.
					prev->next = next;

					if (item->next!=NULL)
					{
						// not the last in the list
						next->prev = prev;// backwards link the next item to the item before us in the list.
					}

					item->next = LinkHead;// link our next item to the item at the head of the list

					LinkHead->prev = item;// backwards link the hold head item to the item moving to the top of the list.

					item->prev=NULL;// change the items prev to NULL now we are at teh top of the list
					LinkHead = item;// Change the global for the head of the link to the item that is to be at the top of the list.
				}
			}
			else
			{
				// Not in the list
				item = LinkHead;// setup to traverse the list from the top.

				// need to use the last item in the list as the new item at the top of the list.
				// find last item in the list
				while(item->next != NULL )
				{
					item=item->next;
				}
				//item is now the last

				(item->prev)->next = NULL;// make the previous item the last

				LinkHead->prev = item;// set the current head item to back reference this item.
				item->next = LinkHead;// set this items next to the current head
				LinkHead = item;// Make this item the new head

				item->id=id;
				item->talkGroup =  talkGroup;
				memset(item->talkerAlias,0,32);// Clear any TA data
			}
		}
	}
	else
	{
		int TAStartPos;
		int TABlockLen;
		int TAOffset;

		// Data contains the Talker Alias Data
		switch(tmp_ram[0])
		{
			case 0x04:
				TAOffset=0;
				TAStartPos=3;
				TABlockLen=6;
				break;
			case 0x05:
				TAOffset=6;
				TAStartPos=2;
				TABlockLen=7;
				break;
			case 0x06:
				TAOffset=13;
				TAStartPos=2;
				TABlockLen=7;
				break;
			case 0x07:
				TAOffset=20;
				TAStartPos=2;
				TABlockLen=7;
				break;
			default:
				TAOffset=0;
				TAStartPos=0;
				TABlockLen=0;
				break;
		}
		if (LinkHead->talkerAlias[TAOffset] == 0x00 && TABlockLen!=0)
		{
			memcpy(&LinkHead->talkerAlias[TAOffset],&tmp_ram[TAStartPos],TABlockLen);// Brandmeister seems to send callsign as 6 chars only
			menuDisplayQSODataState=QSO_DISPLAY_CALLER_DATA;
		}
	}
}

// Needed to convert the legacy DMR ID data which uses BCD encoding for the DMR ID numbers
int int2bcd(int i)
{
    int result = 0;
    int shift = 0;

    while (i)
    {
        result +=  (i % 10) << shift;
        i = i / 10;
        shift += 4;
    }
    return result;
}

bool dmrIDLookup( int targetId,dmrIdDataStruct_t *foundRecord)
{
	uint32_t l = 0;
	uint32_t numRecords;
	uint32_t r;
	uint32_t m;
	uint32_t recordLenth;//15+4;
	uint8_t headerBuf[32];
	memset(foundRecord,0,sizeof(dmrIdDataStruct_t));

	int targetIdBCD=int2bcd(targetId);

	SPI_Flash_read(DMRID_MEMORY_STORAGE_START,headerBuf,DMRID_HEADER_LENGTH);

	if (headerBuf[0] != 'I' || headerBuf[1] != 'D' || headerBuf[2] != '-')
	{
		return false;
	}

	numRecords = (uint32_t) headerBuf[8] | (uint32_t) headerBuf[9] << 8 | (uint32_t)headerBuf[10] <<16 | (uint32_t)headerBuf[11] << 24 ;

	recordLenth = (uint32_t) headerBuf[3] - 0x4a;

	r = numRecords - 1;

	while (l <= r)
	{
		m = (l + r) >> 1;

		SPI_Flash_read((DMRID_MEMORY_STORAGE_START+DMRID_HEADER_LENGTH) + recordLenth*m,(uint8_t *)foundRecord,recordLenth);

		if (foundRecord->id < targetIdBCD)
		{
			l = m + 1;
		}
		else
		{
			if (foundRecord->id >targetIdBCD)
			{
				r = m - 1;
			}
			else
			{
				return true;
			}
		}
	}
	sprintf(foundRecord->text,"ID:%d",targetId);
	return false;
}



void menuUtilityRenderQSOData()
{
char buffer[32];// buffer passed to the DMR ID lookup function, needs to be large enough to hold worst case text length that is returned. Currently 16+1
dmrIdDataStruct_t currentRec;

	sprintf(buffer,"TG %d", LinkHead->talkGroup);
	UC1701_printCentered(16, buffer,UC1701_FONT_GD77_8x16);

	// first check if we have this ID in the DMR ID data
	if (dmrIDLookup( LinkHead->id,&currentRec))
	{
		sprintf(buffer,"%s", currentRec.text);
		UC1701_printCentered(32, buffer,UC1701_FONT_GD77_8x16);
	}
	else
	{
		// We don't have this ID, so try looking in the Talker alias data
		if (LinkHead->talkerAlias[0] != 0x00)
		{
			if (strlen(LinkHead->talkerAlias)> 16)
			{
				// More than 1 line wide of text, so we need to split onto 2 lines.
				memcpy(buffer,LinkHead->talkerAlias,16);
				buffer[16]=0x00;
				UC1701_printCentered(32, buffer,UC1701_FONT_GD77_8x16);

				memcpy(buffer,&LinkHead->talkerAlias[16],16);
				buffer[16]=0x00;
				UC1701_printAt(0,48,buffer,UC1701_FONT_GD77_8x16);
			}
			else
			{
				UC1701_printCentered(32,LinkHead->talkerAlias,UC1701_FONT_GD77_8x16);
			}
		}
		else
		{
			// No talker alias. So we can only show the ID.
			sprintf(buffer,"ID: %d", LinkHead->id);
			UC1701_printCentered(32, buffer,UC1701_FONT_GD77_8x16);
		}
	}
}

void menuUtilityRenderHeader()
{
	char buffer[16];
	if (open_squelch)
	{
		UC1701_printAt(0, 8, "SQ:OFF",1);
	}
	else
	{
		UC1701_printAt(0, 8, "SQ:ON ",1);
	}
	if (HR_C6000_datalogging)
	{
		UC1701_printAt(14*6, 8, "LOG:ON ",1);
	}
	else
	{
		UC1701_printAt(14*6, 8, "LOG:OFF",1);
	}


	sprintf(buffer,"%s", trxGetMode() == RADIO_MODE_ANALOG?"FM":"DMR");
	UC1701_printCentered(8, buffer,1);
}
