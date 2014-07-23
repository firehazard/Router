package jpcap.packet;

import jpcap.JpcapCaptor;

/** This is a root class of the all the packets captured by {@link JpcapCaptor Jpcap}. */
public class Packet
{
	/** Captured timestamp (sec) */
	public long sec;
	
	/** Captured timestamp (micro sec) */
	public long usec;
	
	/** Captured length */
	public int caplen;
	
	/** Length of this packet */
	public int len;
	
	/** Datalink layer header */
	public DatalinkPacket datalink;

	/** Header data */
	public byte[] header;

	/** Packet data (excluding the header) */
	public byte[] data;

	void setPacketValue(long sec,long usec,int caplen,int len){
		this.sec=sec;this.usec=usec;
		this.caplen=caplen;
		this.len=len;
	}

	void setDatalinkPacket(DatalinkPacket p){
		datalink=p;
	}
	
	void setPacketData(byte[] data){
		this.data=data;
	}
	
	void setPacketHeader(byte[] header){
		this.header=header;
	}
	
	/** Returns a string representation of this packet<BR>
         * Format: sec:usec
         * @return a string representation of this packet
         */
	public String toString(){
		return sec+":"+usec;
	}
}
