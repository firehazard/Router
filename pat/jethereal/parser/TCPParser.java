package jethereal.parser;

import jethereal.EtherealTreeNode;

public class TCPParser extends PacketParser {

	TCPParser() {
		dataLen = 1;
		dataStart = 20;
		typeLen = 2;
		typeStart = 2;
	}
	
	protected byte[] strip(byte[] packet) {
		int headerLen = ((packet[12] & 0xF0) >> 4) * 4;
		byte[] ret = new byte[packet.length - headerLen];
		System.arraycopy(packet, headerLen, ret, 0, ret.length);
		return ret;
	}
	
	@Override
	public String getProtocol(byte[] packet) {
		// Look inside 
		if (strip(packet).length == 0)
			return "TCP";
		else
			return "TCP (" + Registry.lookup("TCP", getType(packet)).getProtocol(strip(packet)) + ")";
	}

	@Override
	public String getInfo(byte[] packet) {
		String flags = "";
		if ((packet[13] & 0x20) != 0) flags += "URG "; 
		if ((packet[13] & 0x10) != 0) flags += "ACK "; 
		if ((packet[13] & 0x08) != 0) flags += "PSH "; 
		if ((packet[13] & 0x04) != 0) flags += "RST "; 
		if ((packet[13] & 0x02) != 0) flags += "SYN "; 
		if ((packet[13] & 0x01) != 0) flags += "FIN "; 
		
		if (!flags.equals(""))
			flags = "(TCP " + flags + ") ";
		
		if (strip(packet).length != 0)
			flags += Registry.lookup("TCP", getType(packet)).getInfo(strip(packet)) + " (Port " + getType(packet) + ")";
		
		return flags;
	}

	@Override
	public EtherealTreeNode getTree(byte[] packet) {
		EtherealTreeNode root = new EtherealTreeNode("TCP Packet (" + packet.length +" bytes)", packet);

		int len = ((packet[12] & 0xF0) >> 2);
		dataStart = len;
		
		EtherealTreeNode header = new EtherealTreeNode("TCP Packet Header (" + len + " bytes)", 0, len);
		root.add(header);
		
		int src_port = ((packet[0] & 0xFF) << 8) | ((packet[1] & 0xFF));
		int dst_port = ((packet[2] & 0xFF) << 8) | ((packet[1] & 0xFF));
		long seq_num = ((packet[3] & 0xFF) << 24) | ((packet[4] & 0xFF) << 16) |
						((packet[5] & 0xFF) << 8) | (packet[6] & 0xFF);
		long ack_num = ((packet[7] & 0xFF) << 24) | ((packet[8] & 0xFF) << 16) |
						((packet[9] & 0xFF) << 8) | (packet[10] & 0xFF);
		header.add(makeNode("Src Port = " + src_port, 0, 2));
		header.add(makeNode("Dst Port = " + dst_port, 2, 2));
		header.add(makeNode("Sequence Number = " + seq_num, 4, 4));
		header.add(makeNode("Aknowledgement Number = " + ack_num, 8, 4));
		
		header.add(makeNode("Header Size", len, 12, 1));
		
		String ecn = "";
		if ((packet[12] & 0x01) != 0) ecn += "Nonce Sum "; 
		if ((packet[13] & 0x80) != 0) ecn += "CWR "; 
		if ((packet[13] & 0x40) != 0) ecn += "ECE "; 
		if ((packet[13] & 0x20) != 0) ecn += "URG "; 
		if ((packet[13] & 0x10) != 0) ecn += "ACK "; 
		if ((packet[13] & 0x08) != 0) ecn += "PSH "; 
		if ((packet[13] & 0x04) != 0) ecn += "RST "; 
		if ((packet[13] & 0x02) != 0) ecn += "SYN "; 
		if ((packet[13] & 0x01) != 0) ecn += "FIN "; 
		
		header.add(makeNode("Control Bits = " + ecn, 13, 1));

		header.add(makeNode("Window", packet, 14, 2));
		header.add(makeNode("Checksum = 0x" + hexFormat(packet, 16, 2), 16, 2));
		header.add(makeNode("Urgent Pointer", packet, 18, 2));
		
		header.add(makeNode("Options and Padding", 20, len - 20));

		if (strip(packet).length != 0)
			root.add(nextNode(packet, "TCP", len));
		
		return root;
	}


}
