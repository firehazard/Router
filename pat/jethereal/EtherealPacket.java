package jethereal;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Enumeration;

import javax.swing.JTree;
import javax.swing.table.TableModel;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.DefaultTreeModel;
import javax.swing.tree.MutableTreeNode;
import javax.swing.tree.TreeNode;
import javax.swing.tree.TreePath;

import jethereal.parser.PacketParser;
import jethereal.parser.Registry;
import jpcap.packet.Packet;


public class EtherealPacket {

	/* Cached Display Data */
	private Object[] headerData = new Object[EtherealTableModel.columnNames.length];
	private EtherealTreeNode[] corelation;
	
	private DefaultMutableTreeNode tree = null;

	private TableModel hexTable = null;

	private byte[] packet;
	private long time;
	private int originalLength;

	private PacketParser parser;

	private static int curNum = 0;
	public static void clear() {curNum = 0;} 
	
	private static DateFormat timeFormat = new SimpleDateFormat("HH:mm:ss.SSS MMM dd, yyyy");
	
	public EtherealPacket(byte[] p, long time, int originalLength) {
		int num;
		this.packet = p;
		num = curNum++;
		this.time = time;
		this.originalLength = originalLength;
		
		headerData[0] = num;
		headerData[1] = timeFormat.format(new Date(time/1000));
		headerData[2] = originalLength;
		
		// Asume we are an ethernet packet for now
		parser = Registry.lookup("Ethernet", null);

		headerData[3] = parser.getSrc(getArray());
		headerData[4] = parser.getDst(getArray());
		headerData[5] = parser.getProtocol(getArray());
		headerData[6] = parser.getInfo(getArray());
		
	}

	public byte[] getArray() {
		return packet;
	}

	public Object getRowData(int columnIndex) {
		return headerData[columnIndex];
	}

	public TableModel getTableModel() {
		if (hexTable == null)
			hexTable = parser.getTableModel(getArray());
		return hexTable;
	}

	@SuppressWarnings("unchecked")
	public TreeNode getTree() {
		if (tree == null) {
			tree = new EtherealTreeNode("Packet (size " + packet.length
					+ " bytes)", getArray());
			MutableTreeNode n = parser.getTree(getArray());
			if (n != null)
				tree.add(n);
		}
		
		corelation = new EtherealTreeNode[packet.length];
		Enumeration<EtherealTreeNode> en = tree.breadthFirstEnumeration();
		while (en.hasMoreElements()) {
			EtherealTreeNode node = en.nextElement();
			for (int i=node.hexStart; i<node.hexEnd; i++) {
				corelation[i] = node;
			}
		}
		
		return tree;
	}

	public void hexSelectionChanged(int row, int col, JTree tree, HexTable hexTable) {
		if (row == -1 || col == -1) return;
		if (row * 16 + col >= packet.length) return;
		
		EtherealTreeNode node = corelation[row * 16 + col];
		TreePath path = new TreePath(((DefaultTreeModel) tree.getModel() ).getPathToRoot(node));
		
		tree.setSelectionPath(path);
		
		treeSelectionChanged(node, hexTable);
	}

	public void treeSelectionChanged(EtherealTreeNode node, HexTable hexTable) {

		//hexTable.clearSelection();
		hexTable.resetRenderers();

		int row1 = node.getHexStart() / 16;
		int col1 = node.getHexStart() - 16 * row1;
		hexTable.setCellRenderer(row1, col1);
		int row2 = node.getHexEnd() / 16;
		int col2 = node.getHexEnd() - 16 * row2;

		if (row1 == row2)
			for (int i = col1; i < col2; i++)
				hexTable.setCellRenderer(row2, i);
		else {

			for (int i = col1; i < 16; i++)
				hexTable.setCellRenderer(row1, i);

			for (int i = row1 + 1; i < row2; i++)
				for (int j = 0; j < 16; j++)
					hexTable.setCellRenderer(i, j);

			for (int i = 0; i < col2; i++)
				hexTable.setCellRenderer(row2, i);
		}

	}

	public Packet getJPCapPacket() {
		Packet p = new Packet();
		p.sec = time/1000000;
		p.usec = time % 1000000;
		p.caplen = packet.length;
		p.len = originalLength;
		p.caplen = packet.length;
		p.data = packet;
		return p;
	}

	public int getOriginalLength() {
		return originalLength;
	}

	public long getTime() {
		return time;
	}

}
