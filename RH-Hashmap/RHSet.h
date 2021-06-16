
#pragma once

#include <iostream>
#include <cstdint>
#include <functional>
#include <limits>
#include <assert.h>
#include <new>

namespace NRobinHoodSet {

    // EntryT is TableEntryImpl<T>, with T being the type stored in this set
    template<typename EntryT>
    class IteratorImpl {
    public:
        // DataT is T
        using DataT = typename std::remove_reference< decltype(std::declval<EntryT>().Data()) >::type;

        IteratorImpl(EntryT* pEntry);

        IteratorImpl& operator++();

        DataT& operator*() { return _pEntry->Data(); }
        const DataT& operator*() const { return _pEntry->Data(); }
        bool operator==( const IteratorImpl& Other ) const { return _pEntry == Other._pEntry; }
        bool operator!=( const IteratorImpl& Other ) const { return _pEntry != Other._pEntry; }

    private:
        EntryT* _pEntry;
    };

    ////////////////////////////////////////////////////////////////////////////////

    // T is the type stored in the RH set
    template <typename T>
    class TableEntryImpl
    {
        // Stored as union to automatically handle storage/alignment 
        union DataWrapper
        {
            // This leaves _Value uninitialized on purpose (it is constructed with placement-new)
            DataWrapper() {}
            ~DataWrapper() {}
            T _Value;
        };

        // value of empty entries
        static constexpr uint8_t EMPTY_ID = std::numeric_limits<uint8_t>::max();
        // value of entry guarding list allocation end
        static constexpr uint8_t FINAL_IDX = std::numeric_limits<uint8_t>::max() - 1;

    public:
        TableEntryImpl() : _IdxOffset(EMPTY_ID) {}

        template<typename KeyT>
        void CreateData(KeyT&& Key, uint8_t IdxOffset);
        void CreateData(TableEntryImpl&& Other, uint8_t IdxOffset);

        bool IsEmpty() const { return _IdxOffset == EMPTY_ID; }
        bool IsEndSentinel() const { return _IdxOffset == FINAL_IDX; }
        T& Data() { return _Data._Value; }
        const T& Data() const { return _Data._Value; }
        uint8_t GetIdxOffset() const { return _IdxOffset; }

        void MakeIntoEndSentinel() { _IdxOffset = FINAL_IDX; }
        void SetIdxOffsetToEmpty() { _IdxOffset = EMPTY_ID; }
    private:

        DataWrapper _Data;
        uint8_t _IdxOffset = EMPTY_ID;
    };

    ////////////////////////////////////////////////////////////////////////////////

    // Robin hood set always growing in powers of 2. 
    // T has to have a std::hash function, an equality comparison function, and a copy/move constructor.
    template<typename T>
    class RobinHoodSet {
    public:
        using TableEntry = TableEntryImpl<T>;
        using Iterator = IteratorImpl<TableEntry>;
        using ConstIterator = IteratorImpl<const TableEntry>;

        RobinHoodSet();
        ~RobinHoodSet();
        RobinHoodSet( const RobinHoodSet& ) = delete;
        RobinHoodSet& operator=( const RobinHoodSet& ) = delete;

        // The KeyT type is the same type as T, +/- a ref (required for perfect forwarding)
        template<typename KeyT>
        T& Set( KeyT&& Key );

        // Returns true if an item was removed
        // After removing, will attemp to shift up subsequent entries (this keeps entries packed so that Find() doesn't have to search the full _MaxProbeDist every time)
        bool Remove(const T& Key);

        const T* Find(const T& Key) const;
        bool Contains(const T& Key) const;

        int32_t GetSize() const { return _NumElements; }

        Iterator IteratorBegin() { return Iterator(_pTable); }
        Iterator IteratorEnd() { return Iterator(  &_pTable[_TableTrueSize - 1] ); }
        ConstIterator IteratorBegin() const { return ConstIterator(_pTable); }
        ConstIterator IteratorEnd() const { return ConstIterator( &_pTable[_TableTrueSize - 1] ); }

        // for-each compatibility
        Iterator begin() { return IteratorBegin(); }
        Iterator end() { return IteratorEnd(); }
        ConstIterator begin() const { return IteratorBegin(); }
        ConstIterator end() const { return IteratorEnd(); }

    private:
        void Grow();
        uint32_t CalcDesiredIdx(const T& Val) const;

    private:
        int32_t _NumElements;
        uint8_t _MaxProbeDist;
        int32_t _TableModSize;
        // The true size of the table is its "power of 2" size + extra (log(n)) collision space + 1 (the end sentinel).
        // It would of course be possible to make collisions at the end of the "power of 2" space wrap around, 
        // and thus avoid allocating the extra collision space, but for this implementation the logic is kept simple.
        int32_t _TableTrueSize = _TableModSize + _MaxProbeDist + 1;
        // For code simplicity, even "empty" tables allocate some data. (This could probably be fixed by having a static 
        // allocation that all empty sets points to)
        TableEntry* _pTable = new TableEntry[_TableTrueSize];

        std::hash<T> _KeyHasher;
    };



    ////////////////////////////////////////////////////////////////////////////////
    // Impl


    template<typename EntryT>
    IteratorImpl<EntryT>::IteratorImpl(EntryT* pEntry)
        : _pEntry(pEntry)
    {
        while (_pEntry->IsEmpty())
        {
            ++_pEntry;
        }
    }

    template<typename EntryT>
    IteratorImpl<EntryT>& IteratorImpl<EntryT>::operator++()
    {
        do
        {
            ++_pEntry;
        } while (_pEntry->IsEmpty());
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////

    template <typename T>
    template<typename KeyT>
    void TableEntryImpl<T>::CreateData(KeyT&& Key, uint8_t IdxOffset)
    {
        new (&_Data._Value) T(std::forward<KeyT>(Key));
        _IdxOffset = IdxOffset;
    }

    template <typename T>
    void TableEntryImpl<T>::CreateData(TableEntryImpl&& Other, uint8_t IdxOffset)
    {
        _Data = std::move(Other._Data);
        _IdxOffset = IdxOffset;
    }

    ////////////////////////////////////////////////////////////////////////////////

    template<typename  T>
    RobinHoodSet<T>::RobinHoodSet()
    : _NumElements( 0 )
    , _MaxProbeDist( 0 )
    , _TableModSize( 1 )
    , _TableTrueSize( 1 )
    {
        _pTable = new TableEntry[1];
        _pTable[0].MakeIntoEndSentinel();
    }

    template<typename  T>
    RobinHoodSet<T>::~RobinHoodSet()
    {
        for ( Iterator ItBegin = IteratorBegin(), ItEnd = IteratorEnd(); ItBegin != ItEnd; ++ItBegin) {
            (*ItBegin).~T();
        }
        delete[] _pTable;
    }

    template<typename T>
    template<typename KeyT>
    T& RobinHoodSet<T>::Set( KeyT&& Key )
    {
        uint32_t DesiredIdx = CalcDesiredIdx(Key);
        TableEntry* pEntry = _pTable + DesiredIdx;
        int8_t Offset = 0;   

        // find
        while ( !pEntry->IsEmpty() && Offset <= pEntry->GetIdxOffset() && Offset < _MaxProbeDist )
        {
            if ( pEntry->Data() == Key )
            {
                // already exists
                return pEntry->Data();
            }
            pEntry++;
            Offset++;
        }

        if ( pEntry->IsEmpty() )
        {
            pEntry->CreateData( std::forward<KeyT>(Key), Offset );
            _NumElements++;
        } else {
            if ( Offset >= _MaxProbeDist ) {
                Grow();
                return Set(std::forward<KeyT>(Key));
            } else {
                assert( Offset > pEntry->GetIdxOffset() );
                // Time for roobin hood: steal this spot
                // find end of this sequence of entries, we need to shift them all down one step to make room
                TableEntry* pSequenceEnd = pEntry + 1;
                while ( !pSequenceEnd->IsEmpty() ) {
                    if ( pSequenceEnd->IsEndSentinel() || pSequenceEnd->GetIdxOffset() >= _MaxProbeDist - 1 ) { // TODO: the -1 can probably be removed?
                        Grow();
                        return Set( std::forward<KeyT>( Key ) );
                    }
                    pSequenceEnd++;
                }
                assert( pSequenceEnd->IsEmpty() );
                // now, shift every element to the right
                while ( pSequenceEnd != pEntry ) {
                    TableEntry* pPrevEntry = pSequenceEnd - 1;
                    uint8_t NewIdxOffset = pPrevEntry->GetIdxOffset() + 1;
                    if ( NewIdxOffset >= _MaxProbeDist )
                    {
                        // To keep logic clean, we don't try to recover from cascading probe length errors
                        Grow();
                        return Set( std::forward<KeyT>( Key ));
                    }
                    pSequenceEnd->CreateData( std::move( *pPrevEntry ), NewIdxOffset );
                    pSequenceEnd = pPrevEntry;
                }
                // finally, insert our value
                pEntry->CreateData( std::forward<KeyT>(Key), Offset );
                _NumElements++;
            }
        }
        return pEntry->Data();
    }

    template<typename T>
    bool RobinHoodSet<T>::Remove(const T& Key)
    {
        uint32_t DesiredIdx = CalcDesiredIdx(Key);
        TableEntry* pEntry = _pTable + DesiredIdx;
        int8_t Offset = 0;

        while (!pEntry->IsEmpty() && Offset <= pEntry->GetIdxOffset() && Offset < _MaxProbeDist)
        {
            if (pEntry->Data() == Key)
            {
                // remove
                pEntry->Data().~T();
                _NumElements--;
                // shift following entries up one step (as long as they are offset > 0).
                // Without this, on subsequent Find()'s we would have to search the full _MaxProbeDist every time.
                TableEntry* pNextEntry = pEntry + 1;
                while (!pNextEntry->IsEmpty() && pNextEntry->GetIdxOffset() > 0 && !pNextEntry->IsEndSentinel())
                {
                    pEntry->CreateData(std::move(*pNextEntry ), pNextEntry->GetIdxOffset() - 1);
                    pEntry++;
                    pNextEntry++;
                }
                pEntry->SetIdxOffsetToEmpty();

                return true;
            }
            pEntry++;
            Offset++;
        }
        return false;
    }

    template<typename T>
    const T* RobinHoodSet<T>::Find(const T& Key) const
    {
        uint32_t DesiredIdx = CalcDesiredIdx(Key);
        TableEntry* pEntry = _pTable + DesiredIdx;
        int8_t Offset = 0;

        while (!pEntry->IsEmpty() && Offset <= pEntry->GetIdxOffset() && Offset < _MaxProbeDist)
        {
            if (pEntry->Data() == Key)
            {
                return &pEntry->Data();
            }
            pEntry++;
            Offset++;
        }

        return nullptr;
    }

    template<typename T>
    bool RobinHoodSet<T>::Contains(const T& Key) const
    {
        return Find(Key) != nullptr;
    }

    template<typename T>
    void RobinHoodSet<T>::Grow()
    {
        TableEntry* pOldTable = _pTable;
        Iterator OldItBegin = IteratorBegin();
        Iterator OldItEnd = IteratorEnd();

        _NumElements = 0;
        int32_t SmallestTableSizePower = 3;
        // TODO: Handle overflow
        _MaxProbeDist = std::max(SmallestTableSizePower, _MaxProbeDist + 1 );
        _TableModSize = 1U << _MaxProbeDist;
        _TableTrueSize = _TableModSize + _MaxProbeDist + 1;
        _pTable = new TableEntry[_TableTrueSize];
        _pTable[_TableTrueSize-1].MakeIntoEndSentinel();

        // Move all entries
        for (; OldItBegin != OldItEnd; ++OldItBegin ) {
            IteratorImpl<TableEntryImpl<T>> test = OldItBegin;
            T& OldData = *OldItBegin;
            Set( std::move( OldData ) );
            OldData.~T();
        }

        delete[] pOldTable;
    }

    template<typename T>
    uint32_t RobinHoodSet<T>::CalcDesiredIdx(const T& Key) const
    {
        // Since this RH table always grows in powers of two, (_TableModSize - 1) will be an efficient bitmask 
        // to map random numbers to "buckets".
        // This, of course, only works well if the hash spreads entropy somewhat evenly over the bits.
        return _KeyHasher(Key) & (_TableModSize - 1);
    }
}
