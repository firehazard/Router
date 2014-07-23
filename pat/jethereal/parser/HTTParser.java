package jethereal.parser;

import jethereal.EtherealTreeNode;

public class HTTParser extends PacketParser {

	@Override
	public String getProtocol(byte[] packet) {
		return "HTTP";
	}

	@Override
	public String getInfo(byte[] packet) {
		// TODO Auto-generated method stub
		return "HTTP Request: " + new String(packet);
	}

	@Override
	public EtherealTreeNode getTree(byte[] packet) {
		// TODO Auto-generated method stub
		EtherealTreeNode root = new EtherealTreeNode("HTTP Packet (" + packet.length +" bytes)", packet);
		
		return root;
	}

}
