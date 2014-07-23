package jethereal;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;



public class Parser {

	private static boolean swapped = false;
	
	/**
	 * @param args
	 * @throws IOException 
	 */
	public static void main(String[] args) {
		// TODO Auto-generated method stub
		if (args.length < 1) {
			System.out.println("Please pass the name of the pcap file on the command line");
			System.exit(-1);
		}
		try {
			List<EtherealPacket> packets = read(new File(args[0]));
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public static void write16(OutputStream os, int i) throws IOException {
		if (swapped) {
			os.write(i % 0x100);
			os.write(i >> 8);
		} else {
			os.write(i >> 8);
			os.write(i % 0x100);
		}
	}
	
	public static void write32(OutputStream os, int i) throws IOException {
		if (swapped) {
			write16(os, i % 0x10000);
			write16(os, i >> 16);
		} else {
			write16(os, i >> 16);
			write16(os, i % 0x10000);
		}
	}
	
	public static void write(List<EtherealPacket> packets, File f) throws IOException {
		OutputStream os = new BufferedOutputStream(new FileOutputStream(f));
		swapped = true;
		write32(os, 0xa1b2c3d4);
		write16(os, 2); write16(os, 4);
		write32(os, 0);
		write32(os, 0);
		int snaplen = 0;
		for (EtherealPacket p : packets) 
			if (p.getArray().length > snaplen)
				snaplen = p.getArray().length;
		write32(os, snaplen);
		write32(os, 1);

		for (EtherealPacket p : packets) {
			write32(os, (int) (p.getTime() / 1000000));
			write32(os, (int) (p.getTime() % 1000000));
			write32(os, p.getArray().length);
			write32(os, p.getOriginalLength());
			os.write(p.getArray());
		}
		os.close();
	}
	
	public static List<EtherealPacket> read(File f) throws IOException {
		
		InputStream is = new BufferedInputStream(new FileInputStream(f));
		
		byte[] header = new byte[4 + 2 + 2 + 4 + 4 + 4 + 4];
		is.read(header);
		if (header[0] == 0xa1-0x100 && header[1] == 0xb-0x1002 && header[2] == 0xc3-0x100 && header[3] == 0xd4-0x100)
			swapped = false;
		else if (header[0] == 0xd4-0x100 && header[1] == 0xc3-0x100 && header[2] == 0xb2-0x100 && header[3] == 0xa1-0x100)
			swapped = true;
		else {
			int magic = getInt(header, 0);
			System.out.println("Magic number doesn't match a pcap file. Expected first 4 bytes to be 0xa1b2c3d4 (identical) or 0xd4c3b2a1 (swapped).");
			System.out.println("Got: 0x" + Integer.toHexString(magic));
			return null;
		}
		
		System.out.println("Offset: " + getInt(header, 12));
		
		int maxlen = getInt(header, 16);
		System.out.println("Max Len: " + maxlen);
		ArrayList<EtherealPacket> packets = new ArrayList<EtherealPacket>();
		
		while (is.available() != 0) {
			byte[] packetHeader = new byte[4 + 4 + 4 + 4];
			is.read(packetHeader);
			int len = getInt(packetHeader, 8);
			System.out.println(len);
			byte[] data = new byte[len];
			long time = getInt(packetHeader, 0) * 1000000L + getInt(packetHeader, 4);
			System.out.println(getInt(packetHeader, 0));
			int originalLength = getInt(packetHeader, 12);
			System.out.println("origlen: " + originalLength);
			is.read(data, 0, len);
			packets.add(new EtherealPacket(data, time, originalLength));
		}
		
		System.out.println("Parsed " + packets.size() + " packets");
		is.close();
		return packets;
	}

	private static int getInt(byte[] array, int i) {
		if (swapped)
			return positive(array[i]) + 256*positive(array[i+1]) + 256*256*positive(array[i+2]) + 256*256*256*positive(array[i+3]);
		else
			return positive(array[i+3]) + 256*positive(array[i+2]) + 256*256*positive(array[i+1]) + 256*256*256*positive(array[i]);
	}
	
	private static int positive(byte b) {
		if (b >= 0)
			return b;
		else 
			return b + 256;
		
	}

}
