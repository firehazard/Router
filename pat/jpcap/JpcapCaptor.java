package jpcap;

import jpcap.packet.Packet;

/**
 * This class is used to capture packets or read packets from a captured file.
 */
public class JpcapCaptor extends JpcapInstance{
	/**
	 * Number of received packets
	 * 
	 * @see #updateStat()
	 */
	public int received_packets;

	/**
	 * Number of dropped packets
	 * 
	 * @see #updateStat()
	 */
	public int dropped_packets;

	private native String nativeOpenLive(String device, int snaplen,
			int promisc, int to_ms);

	private native String nativeOpenOffline(String filename);

	private native void nativeClose();

	private JpcapCaptor() throws java.io.IOException {
		if (reserveID()<0)
			throw new java.io.IOException("Unable to open a device: "
					+ MAX_NUMBER_OF_INSTANCE + " devices are already opened.");
	}

	/**
	 * Returns the interfaces that can be used for capturing.
	 * 
	 * @return List of Interface objects
	 */
	public static native NetworkInterface[] getDeviceList();

	/**
	 * Initializes the network interface, and returns an instance of this class.
	 * 
	 * @return an instance of this class Jpcap.
	 * @param intrface
	 *            The network interface to capture packets
	 * @param snaplen
	 *            Max number of bytes captured at once
	 * @param promisc
	 *            If true, the inferface becomes promiscuous mode
	 * @param to_ms
	 *            Timeout of
	 *            {@link #processPacket(int,PacketReceiver) processPacket()}
	 * @exception java.io.IOException
	 *                Raised when the specified interface cannot be opened
	 */
	public static JpcapCaptor openDevice(NetworkInterface intrface,
			int snaplen, boolean promisc, int to_ms) throws java.io.IOException {
		JpcapCaptor jpcap = new JpcapCaptor();
		String ret = jpcap.nativeOpenLive(intrface.name, snaplen, (promisc ? 1
				: 0), to_ms);

		if (ret != null) // error
			throw new java.io.IOException(ret);

		return jpcap;
	}

	/**
	 * Opens a dump file created by tcpdump or Ethereal, and returns an instance
	 * of this class.
	 * 
	 * @param filename
	 *            File name of the dump file
	 * @exception java.io.IOException
	 *                If the file cannot be opened
	 * @return an instance of this class Jpcap
	 */
	public static JpcapCaptor openFile(String filename)
			throws java.io.IOException {
		JpcapCaptor jpcap = new JpcapCaptor();
		String ret = jpcap.nativeOpenOffline(filename);

		if (ret != null) // error
			throw new java.io.IOException(ret);

		return jpcap;
	}

	/** Closes the opened interface of dump file. */
	public void close() {
		System.out.println("close:"+ID);
		nativeClose();
		unreserveID();
	}

	/**
	 * Returns a captured packet.
	 * 
	 * @return a captured packet
	 */
	public native Packet getPacket();

	/**
	 * Captures the specified number of packets consecutively.
	 * 
	 * @param count
	 *            Number of packets to be captured<BR>
	 *            You can specify -1 to capture packets parmanently until
	 *            timeour, EOF or an error occurs.
	 * @param handler
	 *            an instnace of JpcapHandler that analyzes the captured packets
	 * @return Number of captured packets
	 */
	public native int processPacket(int count, PacketReceiver handler);

	/**
	 * Captures the specified number of packets consecutively.
	 * <P>
	 * 
	 * Unlike processPacket(), this method ignores the timeout.
	 * 
	 * @param count
	 *            Number of packets to be captured<BR>
	 *            You can specify -1 to capture packets parmanently until EOF or
	 *            an error occurs.
	 * @param handler
	 *            an instnace of JpcapHandler that analyzes the captured packets
	 * @return Number of captured packets
	 */
	public native int loopPacket(int count, PacketReceiver handler);

	/**
	 * Captures a group of packets.
	 * <P>
	 * 
	 * Unlike processPacket(), this method may return before collecting the
	 * number of packets specified by count. Also, in "non-blocking" mode, this
	 * method returns immediately even if it didn't capture any packet.
	 * loopPacket() and processPacket() do not support "non-blocking" mode.
	 * 
	 * @param count
	 *            Number of packets to be captured<BR>
	 *            You can specify -1 to capture packets parmanently until EOF or
	 *            an error occurs.
	 * @param handler
	 *            an instnace of JpcapHandler that analyzes the captured packets
	 * @return Number of captured packets
	 */
	public native int dispatchPacket(int count, PacketReceiver handler);

	/**
	 * Sets/unsets "non-blocking" mode
	 * @param nonblocking TRUE to set "non-blocking" mode. FALSE to set "blocking" mode
	 */
	public native void setNonBlockingMode(boolean nonblocking);
	
	/**
	 * Checks if the current setting is in "non-blocking" mode or not.
	 * @return TRUE if it is in "non-blocking" mode. FALSE otherwise.
	 */
	public native boolean isNonBlockinMode();
	
	/**
	 * Set a flag that will force processPacket() and loopPacket() to return
	 * rather than looping.
	 * 
	 */
	public native void breakLoop();

	/**
	 * Sets a filter. This filter is same as tcpdump.
	 * 
	 * @param condition
	 *            a string representation of the filter
	 * @param optimize
	 *            If true, the filter is optimized
	 */
	public native void setFilter(String condition, boolean optimize);

	/**
	 * Updates {@link #received_packets received_packets} and
	 * {@link #dropped_packets dropped_packets}.
	 */
	public native void updateStat();
	
	/**
	 * Returns an error message
	 * 
	 * @return error message
	 */
	public native String getErrorMessage();

	/**
	 * Obtains an instance of JpcapSender that uses the same interface to send packets.
	 * You can use this method only if you opened an interface with openDevice() method.
	 * @return returns an instance of JpcapSender
	 */
	public JpcapSender getJpcapSenderInstance(){
		return new JpcapSender(ID);
	}
	
	static {
		System.loadLibrary("jpcap");
	}
}
