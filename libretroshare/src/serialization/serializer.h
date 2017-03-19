#pragma once

#include <stdint.h>
#include <string.h>
#include <iostream>
#include <string>
#include "arpa/inet.h"
#include "serialiser/rsserial.h"

class RsItem ;

class SerializeContext;

class RsSerializable: public RsItem
{
public:
	typedef enum { SIZE_ESTIMATE = 0x01, SERIALIZE = 0x02, DESERIALIZE  = 0x03} SerializeJob ;

	RsSerializable(uint8_t version,uint16_t service,uint8_t id)
		: RsItem(version,service,id)
	{
	}

	/**
	 * @brief serialize this object to the given buffer
	 * @param Job to do: serialise or deserialize.
	 * @param data Chunk of memory were to dump the serialized data
	 * @param size Size of memory chunk
	 * @param offset Readed to determine at witch offset start writing data,
	 *        written to inform caller were written data ends, the updated value
	 *        is usually passed by the caller to serialize of another
	 *        RsSerializable so it can write on the same chunk of memory just
	 *        after where this RsSerializable has been serialized.
	 * @return true if serialization successed, false otherwise
	 */

	virtual void serial_process(SerializeJob j,SerializeContext& ctx) = 0;
};

class SerializeContext
{
	public:

	SerializeContext(uint8_t *data,uint32_t size)
		: mData(data),mSize(size),mOffset(0),mOk(true) {}

	unsigned char *mData ;
	uint32_t mSize ;
	uint32_t mOffset ;
	bool mOk ;
};
template<typename T> T ntoh(T t)
{
	if(sizeof(T) == 4) return ntohl(t) ;
	if(sizeof(T) == 2) return ntohs(t) ;

	std::cerr << "(EE) unhandled type of size " << sizeof(T) << " in ntoh<>" << std::endl;
	return t;
}
template<typename T> T hton(T t)
{
	if(sizeof(T) == 4) return htonl(t) ;
	if(sizeof(T) == 2) return htons(t) ;

	std::cerr << "(EE) unhandled type of size " << sizeof(T) << " in hton<>" << std::endl;
	return t;
}

template<typename T> class RsTypeSerializerBase
{
	public:
		void serial_process(RsSerializable::SerializeJob j,SerializeContext& ctx,T& member) 
		{
			switch(j)
			{
				case RsSerializable::SIZE_ESTIMATE: ctx.mSize += serial_size(member) ;
																break ;

				case RsSerializable::DESERIALIZE:   deserialize(ctx.mData,ctx.mSize,ctx.mOffset,member) ;
																break ;

				case RsSerializable::SERIALIZE:     serialize(ctx.mData,ctx.mSize,ctx.mOffset,member) ;
																break ;

				default:
																throw std::runtime_error("Unknown serial job") ;
			}
		}

	protected:
		virtual bool serialize(uint8_t data[], uint32_t size, uint32_t &offset, const T& member)const=0;
		virtual bool deserialize(const uint8_t data[], uint32_t size, uint32_t &offset, T& member) const=0;
		virtual uint32_t serial_size(const T& /* member */)const=0;
};

/// Templates to generate RsSerializer for standard integral types
template<typename N, uint32_t SIZE> class t_SerializerNType : public RsTypeSerializerBase<N>
{
protected:
	bool serialize(uint8_t data[], uint32_t size, uint32_t &offset, const N& member)const
	{
		if (size <= offset || size - offset < SIZE) 
			return false;

		N tmp = hton<N>(member);
		memcpy(data+offset, &tmp, SIZE);
		offset += SIZE;
		return true;
	}
	bool deserialize(const uint8_t data[], uint32_t size, uint32_t &offset, N& member)const
	{
		if (size <= offset || size - offset < SIZE) 
			return false;

		N tmp ;
		memcpy(&tmp, data+offset, SIZE);
		member = ntoh<N>(tmp);
		offset += SIZE;
		return true;
	}

	inline uint32_t serial_size(const N& /* member */)const
	{ 
		return SIZE; 
	}
};

template<typename T> class RsTypeSerializer
{
//	public:
//		virtual void serial_process(RsSerializable::SerializeJob j,SerializeContext& ctx,const T& member)=0; 
//
//	protected:
//		virtual bool serialize(uint8_t data[], uint32_t size, uint32_t &offset, const T& member)const=0;
//		virtual bool deserialize(const uint8_t data[], uint32_t size, uint32_t &offset, T& member)=0;
//		virtual uint32_t serial_size(const T& /* member */)const=0;
};

template<> class RsTypeSerializer<uint8_t>  : public t_SerializerNType<uint8_t,  1>{};
template<> class RsTypeSerializer<uint16_t> : public t_SerializerNType<uint16_t, 2>{};
template<> class RsTypeSerializer<uint32_t> : public t_SerializerNType<uint32_t, 4>{};
template<> class RsTypeSerializer<uint64_t> : public t_SerializerNType<uint64_t, 8>{};
template<> class RsTypeSerializer<time_t>   : public t_SerializerNType<uint64_t, 8>{};

/// Serializer for <b>non negative</b> float
template<> class RsTypeSerializer<float> : public RsTypeSerializerBase<float>
{
protected:
	bool serialize(uint8_t data[], uint32_t size, uint32_t &offset, const float& f)const
	{
		uint32_t sz = serial_size(f);
		if ( !data || size <= offset || size - offset < sz )
			return false;

		const float tmp = f;
		if(tmp < 0.0f)
		{
			std::cerr << "(EE) Cannot serialise invalid negative float value "
			          << tmp << " in " << __PRETTY_FUNCTION__ << std::endl;
			return false;
		}

		/* This serialisation is quite accurate. The max relative error is approx.
		 * 0.01% and most of the time less than 1e-05% The error is well distributed
		 * over numbers also. */
		uint32_t n;
		if(tmp < 1e-7) n = (~(uint32_t)0);
		else n = ((uint32_t)( (1.0f/(1.0f+tmp) * (~(uint32_t)0))));
		n = hton<uint32_t>(n);
		memcpy(data+offset, &n, sz);
		offset += sz;
		return true;
	}

	bool deserialize(const uint8_t data[], uint32_t size, uint32_t &offset, float& f)const
	{
		uint32_t sz = serial_size(f);
		if ( !data || size <= offset ||
		     size - offset < sz )
			return false;

		uint32_t n;
		memcpy(&n, data+offset, sz);
		n = ntoh<uint32_t>(n);
		f = 1.0f/ ( n/(float)(~(uint32_t)0)) - 1.0f;
		return true;
	}
	inline uint32_t serial_size(const float&)const { return 4; }
};

/// Serializer for std::string
template<> class RsTypeSerializer<std::string> : public RsTypeSerializerBase<std::string>
{
	protected:
		bool serialize(uint8_t data[], uint32_t size, uint32_t &offset,const std::string& s)const
		{
			if ( !data || size <= offset || size - offset < serial_size(s))
				return false;

			uint32_t charsLen = s.length();
			uint32_t netLen = hton<uint32_t>(charsLen);
			memcpy(data+offset, &netLen, 4); offset += 4;
			memcpy(data+offset, s.c_str(), charsLen); offset += charsLen;
			return true;
		}
		bool deserialize(const uint8_t data[], uint32_t size,uint32_t& offset,std::string& s)const
		{
			if ( !data || size <= offset || size - offset < 4 ) return false;
			uint32_t charsLen;
			memcpy(&charsLen, data+offset, 4); offset += 4;
			charsLen = ntoh<uint32_t>(charsLen);

			if ( size <= offset || size - offset < charsLen ) return false;
			s.clear();
			s.insert(0, (char*)data+offset, charsLen);
			offset += charsLen;
			return true;
		}
		inline uint32_t serial_size(const std::string& s)const
		{
			return s.length() + 4;
		}
};



class RsSerializer
{
	public:
		/*! create_item  
		 * 	should be overloaded to create the correct type of item depending on the data
		 */
		virtual RsSerializable *create_item(uint16_t service, uint8_t item_sub_id)
		{
			return NULL ;
		}

		RsSerializable *deserialize_item(const uint8_t *data,uint32_t size) 
		{
			uint32_t rstype = getRsItemId(const_cast<void*>((const void*)data)) ;

			RsSerializable *item = create_item(getRsItemService(rstype),getRsItemSubType(rstype)) ;

			if(!item)
			{
				std::cerr << "(EE) cannot deserialise: unknown item type " << std::hex << rstype << std::dec << std::endl;
				return NULL ;
			}
			
			SerializeContext ctx(const_cast<uint8_t*>(data),size);
			item->serial_process(RsSerializable::DESERIALIZE, ctx) ;

			if(ctx.mOk)
				return item ;

			delete item ;
			return NULL ;
		}

		bool serialize_item(const RsSerializable *item,uint8_t *const data,uint32_t size) 
		{
			SerializeContext ctx(data,0);

			uint32_t tlvsize = size_item(item) ;

			if(tlvsize > size)
				throw std::runtime_error("Cannot serialise: not enough room.") ;

			if(!setRsItemHeader(data, tlvsize, const_cast<RsSerializable*>(item)->PacketId(), tlvsize))
			{
				std::cerr << "RsSerializer::serialise_item(): ERROR. Not enough size!" << std::endl;
				return false ;
			}
			ctx.mOffset = 8;
			ctx.mSize = tlvsize;

			const_cast<RsSerializable*>(item)->serial_process(RsSerializable::SERIALIZE,ctx) ;

			if(ctx.mSize != ctx.mOffset)
			{
				std::cerr << "RsSerializer::serialise_item(): ERROR. offset does not match expected size!" << std::endl;
				return false ;
			}
			return true ;
		}

		uint32_t size_item(const RsSerializable *item) 
		{
			SerializeContext ctx(NULL,0);

			ctx.mSize = 8 ;	// header size
			const_cast<RsSerializable*>(item)->serial_process(RsSerializable::SIZE_ESTIMATE, ctx) ;

			return ctx.mSize ;
		}
};



