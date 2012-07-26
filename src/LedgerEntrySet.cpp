#include "LedgerEntrySet.h"

#include <boost/make_shared.hpp>

LedgerEntrySet LedgerEntrySet::duplicate()
{
	return LedgerEntrySet(mEntries, mSeq + 1);
}

void LedgerEntrySet::setTo(LedgerEntrySet& e)
{
	mEntries = e.mEntries;
	mSeq = e.mSeq;
}

void LedgerEntrySet::swapWith(LedgerEntrySet& e)
{
	std::swap(mSeq, e.mSeq);
	mEntries.swap(e.mEntries);
}

// Find an entry in the set.  If it has the wrong sequence number, copy it and update the sequence number.
// This is basically: copy-on-read.
SLE::pointer LedgerEntrySet::getEntry(const uint256& index, LedgerEntryAction& action)
{
	boost::unordered_map<uint256, LedgerEntrySetEntry>::iterator it = mEntries.find(index);
	if (it == mEntries.end())
	{
		action = taaNONE;
		return SLE::pointer();
	}
	if (it->second.mSeq != mSeq)
	{
		it->second.mEntry = boost::make_shared<SerializedLedgerEntry>(*it->second.mEntry);
		it->second.mSeq = mSeq;
	}
	action = it->second.mAction;
	return it->second.mEntry;
}

LedgerEntryAction LedgerEntrySet::hasEntry(const uint256& index) const
{
	boost::unordered_map<uint256, LedgerEntrySetEntry>::const_iterator it = mEntries.find(index);
	if (it == mEntries.end())
		return taaNONE;
	return it->second.mAction;
}

void LedgerEntrySet::entryCache(SLE::pointer sle)
{
	boost::unordered_map<uint256, LedgerEntrySetEntry>::iterator it = mEntries.find(sle->getIndex());
	if (it == mEntries.end())
	{
		mEntries.insert(std::make_pair(sle->getIndex(), LedgerEntrySetEntry(sle, taaCACHED, mSeq)));
		return;
	}

	switch (it->second.mAction)
	{
		case taaCACHE:
			it->second.mSeq	    = mSeq;
			it->second.mEntry   = sle;
			return;

		default:
			throw std::runtime_error("Cache after modify/delete/create");
	}
}

void LedgerEntrySet::entryCreate(SLE::pointer sle)
{
	boost::unordered_map<uint256, LedgerEntrySetEntry>::iterator it = mEntries.find(sle->getIndex());
	if (it == mEntries.end())
	{
		mEntries.insert(std::make_pair(sle->getIndex(), LedgerEntrySetEntry(sle, taaCREATE, mSeq)));
		return;
	}

	switch (it->second.mAction)
	{
		case taaMODIFY:
			throw std::runtime_error("Create after modify");

		case taaDELETE:
			throw std::runtime_error("Create after delete"); // We could make this a modify

		case taaCREATE:
			throw std::runtime_error("Create after create"); // We could make this work

		case taaCACHED:
			throw std::runtime_error("Create after cache");

		default:
			throw std::runtime_error("Unknown taa");
	}
}

void LedgerEntrySet::entryModify(SLE::pointer sle)
{
	boost::unordered_map<uint256, LedgerEntrySetEntry>::iterator it = mEntries.find(sle->getIndex());
	if (it == mEntries.end())
	{
		mEntries.insert(std::make_pair(sle->getIndex(), LedgerEntrySetEntry(sle, taaMODIFY, mSeq)));
		return;
	}

	switch (it->second.mAction)
	{
		case taaCACHED:
		case taaMODIFY:
			it->second.mSeq	    = mSeq;
			it->second.mEntry   = sle;
			it->second.mAction  = taaMODIFY;
			break;

		case taaDELETE:
			throw std::runtime_error("Modify after delete");

		case taaCREATE:
			it->second.mSeq	    = mSeq;
			it->second.mEntry   = sle;
			break;

		default:
			throw std::runtime_error("Unknown taa");
	}
}

void LedgerEntrySet::entryDelete(SLE::pointer sle)
{
	boost::unordered_map<uint256, LedgerEntrySetEntry>::iterator it = mEntries.find(sle->getIndex());
	if (it == mEntries.end())
	{
		mEntries.insert(std::make_pair(sle->getIndex(), LedgerEntrySetEntry(sle, taaDELETE, mSeq)));
		return;
	}

	switch (it->second.mAction)
	{
		case taaCACHED:
		case taaMODIFY:
			it->second.mSeq	    = mSeq;
			it->second.mEntry   = sle;
			it->second.mAction  = taaDELETE;
			break;

		case taaCREATE:
			mEntries.erase(it);
			break;

		case taaDELETE:
			break;

		default:
			throw std::runtime_error("Unknown taa");
	}
}

// vim:ts=4
