/*************************************************************************
 * Copyright (c) 2013 eProsima. All rights reserved.
 *
 * This copy of FastCdr is licensed to you under the terms described in the
 * EPROSIMARTPS_LIBRARY_LICENSE file included in this distribution.
 *
 *************************************************************************/

/*
 * Subscriber.cpp
 *
 *  Created on: Feb 27, 2014
 *      Author: Gonzalo Rodriguez Canosa
 *      email:  gonzalorodriguez@eprosima.com
 *              grcanosa@gmail.com  	
 */

#include "eprosimartps/dds/Subscriber.h"
#include "eprosimartps/reader/RTPSReader.h"

namespace eprosima {
namespace dds {

Subscriber::Subscriber() {
	// TODO Auto-generated constructor stub

}

Subscriber::Subscriber(RTPSReader* Rin) {
	R = Rin;
	R->newMessageCallback = NULL;
	R->newMessageSemaphore = new boost::interprocess::interprocess_semaphore(0);

}


Subscriber::~Subscriber() {
	// TODO Auto-generated destructor stub
}


void Subscriber::blockUntilNewMessage(){
	R->newMessageSemaphore->wait();
}

void Subscriber::assignNewMessageCallback(void (*fun)()) {
	R->newMessageCallback = fun;
}

bool Subscriber::isHistoryFull()
{
	return R->reader_cache.isFull();
}

int Subscriber::getReadElements_n()
{
	return readElements.size();
}

int Subscriber::getHistory_n()
{
	return R->reader_cache.changes.size();
}

bool Subscriber::isCacheRead(SequenceNumber_t seqNum, GUID_t guid)
{
	std::vector<ReadElement_t>::iterator it;
	for(it=readElements.begin();it!=readElements.end();++it)
	{
		if(seqNum.to64long()== it->seqNum.to64long() &&
				guid == it->writerGuid)
		{
			return true;
		}
	}
	return false;
}

bool Subscriber::readMinSeqUnreadCache(void* data_ptr)
{
	boost::lock_guard<HistoryCache> guard(R->reader_cache);
	if(!R->reader_cache.changes.empty())
	{
		if(R->reader_cache.changes.size() == readElements.size())
		{
			pWarning( "No unread elements " << endl);
			return false; //No elements unread
		}
		SequenceNumber_t minSeqNum;
		GUID_t minSeqNumGuid;
		SEQUENCENUMBER_UNKOWN(minSeqNum);
		std::vector<CacheChange_t*>::iterator it;
		if(readElements.empty()) // No element is read yet
		{
			R->reader_cache.get_seq_num_min(&minSeqNum,&minSeqNumGuid);
		}
		else
		{
			for(it=R->reader_cache.changes.begin();it!=R->reader_cache.changes.end();++it)
			{
				if(!isCacheRead((*it)->sequenceNumber,(*it)->writerGUID))
				{
					if(minSeqNum.high == -1 ||
					(*it)->sequenceNumber.to64long() < minSeqNum.to64long())
					{
						minSeqNum = (*it)->sequenceNumber;
						minSeqNumGuid = (*it)->writerGUID;
					}
				}
			}
		}
		ReadElement_t rElem;
		rElem.seqNum = minSeqNum;
		rElem.writerGuid = minSeqNumGuid;
		readElements.push_back(rElem);
		CacheChange_t* ch = new CacheChange_t();
		R->reader_cache.get_change(minSeqNum,minSeqNumGuid,&ch);
		if(ch->kind == ALIVE)
			type.deserialize(&ch->serializedPayload,data_ptr);
		else
		{
			pWarning("Reading a NOT ALIVE change ");
		}

		delete(ch);
		return true;
	}
	pWarning("Not read anything. Reader Cache empty" << endl);
	return false;
}
bool Subscriber::readCache(SequenceNumber_t sn, GUID_t wGuid,void* data_ptr)
{
	boost::lock_guard<HistoryCache> guard(R->reader_cache);
	CacheChange_t* ch = new CacheChange_t();
	if(R->reader_cache.get_change(sn,wGuid,&ch))
	{
		if(ch->kind == ALIVE)
			type.deserialize(&ch->serializedPayload,data_ptr);
		else
		{
			pWarning("Reading a NOT ALIVE change " << endl);

		}
		if(!isCacheRead(sn,wGuid))
		{
			ReadElement_t r_elem;
			r_elem.seqNum = sn;
			r_elem.writerGuid = wGuid;
			readElements.push_back(r_elem);
		}
		delete(ch);
		return true;
	}
	else
	{
		pWarning("Change not found ");
		delete(ch);
		return false;
	}
}

bool Subscriber::readAllUnreadCache(std::vector<void*>* data_vec)
{
	boost::lock_guard<HistoryCache> guard(R->reader_cache);
	if(!R->reader_cache.changes.empty())
	{
		if(R->reader_cache.changes.size() == readElements.size())
			return false; //No elements unread
		SequenceNumber_t minSeqNum;
		GUID_t minSeqNumGuid;
		SEQUENCENUMBER_UNKOWN(minSeqNum);
		std::vector<CacheChange_t*>::iterator it;
		bool noneRead = false;
		bool read_this = false;
		if(readElements.empty())
			noneRead = true;
		for(it=R->reader_cache.changes.begin();it!=R->reader_cache.changes.end();++it)
		{
			read_this = false;
			if(noneRead)
				read_this = true;
			else //CHEK IF THE ELEMENT WAS ALREADY READ
			{
				if(!isCacheRead((*it)->sequenceNumber,(*it)->writerGUID))
					read_this=true;
			}
			if(read_this)
			{
				ReadElement_t rElem;
				rElem.seqNum = minSeqNum;
				rElem.writerGuid = minSeqNumGuid;
				readElements.push_back(rElem);
				CacheChange_t* ch = new CacheChange_t();
				R->reader_cache.get_change(minSeqNum,minSeqNumGuid,&ch);
				if(ch->kind == ALIVE)
				{
					void * data_ptr = malloc(type.byte_size);
					type.deserialize(&ch->serializedPayload,data_ptr);
					data_vec->push_back(data_ptr);
				}
				else
					{pWarning("Cache with NOT ALIVE" << endl);}
				delete(ch);
			}
		}
	}
	pWarning("No elements in history ");

	return false;
}

bool Subscriber::readMinSeqCache(void* data_ptr,SequenceNumber_t* minSeqNum, GUID_t* minSeqNumGuid)
{
	boost::lock_guard<HistoryCache> guard(R->reader_cache);
	if(!R->reader_cache.changes.empty())
	{
		R->reader_cache.get_seq_num_min(minSeqNum,minSeqNumGuid);
		ReadElement_t rElem;
		rElem.seqNum = *minSeqNum;
		rElem.writerGuid = *minSeqNumGuid;
		if(!isCacheRead(*minSeqNum,*minSeqNumGuid))
			readElements.push_back(rElem);
		CacheChange_t* ch;
		if(R->reader_cache.get_change(*minSeqNum,*minSeqNumGuid,&ch))
		{
			if(ch->kind == ALIVE)
			{

				//boost::posix_time::ptime t1,t2;
				//t1 = boost::posix_time::microsec_clock::local_time();
				type.deserialize(&ch->serializedPayload,data_ptr);
				//t2 = boost::posix_time::microsec_clock::local_time();
				//cout<< "TIME total deserialize operation: " <<(t2-t1).total_microseconds()<< endl;
			}
			else
			{pWarning("Cache with NOT ALIVE" << endl)}
			return true;
		}
		else
		{
			pWarning("Min element not found")
					return false;
		}
	}
	pWarning("No elements in history " << endl);
	return false;
}

bool Subscriber::readAllCache(std::vector<void*>* data_vec)
{
	boost::lock_guard<HistoryCache> guard(R->reader_cache);
	if(!R->reader_cache.changes.empty())
	{
		std::vector<CacheChange_t*>::iterator it;
		std::vector<ReadElement_t> readElem2;
		for(it=R->reader_cache.changes.begin();it!=R->reader_cache.changes.end();++it)
		{
			ReadElement_t r_elem;
			r_elem.seqNum = (*it)->sequenceNumber;
			r_elem.writerGuid = (*it)->writerGUID;
			readElem2.push_back(r_elem);
			if((*it)->kind==ALIVE)
			{
				void * data_ptr = malloc(type.byte_size);
				type.deserialize(&(*it)->serializedPayload,data_ptr);
				data_vec->push_back(data_ptr);
			}
			else
				{pWarning("Cache with NOT ALIVE" << endl);}
		}
		readElements = readElem2;
		return true;
	}
	pWarning("No elements in history " << endl);
	return false;
}


bool Subscriber::takeAllCache(std::vector<void*>* data_vec)
{
	boost::lock_guard<HistoryCache> guard(R->reader_cache);
	if(readAllCache(data_vec))
	{
		R->reader_cache.remove_all_changes();
		readElements.clear();
		return true;
	}
	pWarning("Not all cache read " << endl);

	return false;
}



bool Subscriber::takeMinSeqCache(void* data_ptr)
{
	boost::lock_guard<HistoryCache> guard(R->reader_cache);
	if(!R->reader_cache.changes.empty())
	{
		SequenceNumber_t seq;
		GUID_t guid;
		R->reader_cache.get_seq_num_min(&seq,&guid);

		ReadElement_t rElem;
		rElem.seqNum = seq;
		rElem.writerGuid = guid;
		if(!isCacheRead(seq,guid))
			readElements.push_back(rElem);
		CacheChange_t* change;
		uint16_t ch_number;
		if(R->reader_cache.get_change(seq,guid,&change,&ch_number))
		{


			if(change->kind == ALIVE)
				type.deserialize(&change->serializedPayload,data_ptr);
			else
			{
				pWarning("Cache with NOT ALIVE" << endl)
			}
			delete(change);
			R->reader_cache.changes.erase(R->reader_cache.changes.begin()+ch_number);
			removeSeqFromRead(seq,guid);
			return true;
		}
		else
		{
			pWarning("Min Element not found"<<endl);
			return false;
		}
	}
	pWarning("No elements in history " << endl);
	return false;
}

bool Subscriber::minSeqRead(SequenceNumber_t* sn,GUID_t* guid,std::vector<ReadElement_t>::iterator* min_it)
{
	if(!readElements.empty())
	{
		std::vector<ReadElement_t>::iterator it;
		ReadElement_t minRead;
		minRead.seqNum.high = -1;
		for(it=readElements.begin();it!=readElements.end();++it)
		{
			if(minRead.seqNum.high == -1 ||
				minRead.seqNum.to64long() > it->seqNum.to64long())
			{
				minRead = *it;
				*min_it = it;
			}
		}
		*sn = minRead.seqNum;
		*guid = minRead.writerGuid;
		return true;
	}
	pWarning("No read elements " << endl);

	return false;
}

bool Subscriber::removeSeqFromRead(SequenceNumber_t sn,GUID_t guid)
{
	if(!readElements.empty())
	{
		std::vector<ReadElement_t>::iterator it;

		for(it=readElements.begin();it!=readElements.end();++it)
		{
			if(it->seqNum.to64long()==sn.to64long()&&
					it->writerGuid == guid)
			{
				readElements.erase(it);
				return true;
			}
		}
		return false;
	}
	pWarning("No read elements " << endl);

	return false;
}



} /* namespace dds */
} /* namespace eprosima */

