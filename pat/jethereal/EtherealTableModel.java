package jethereal;

import java.util.ArrayList;

import javax.swing.JTree;
import javax.swing.ListSelectionModel;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;
import javax.swing.event.TreeSelectionEvent;
import javax.swing.event.TreeSelectionListener;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.TableModel;
import javax.swing.tree.DefaultTreeModel;
import javax.swing.tree.TreeModel;
import javax.swing.tree.TreeNode;

public class EtherealTableModel extends AbstractTableModel implements ListSelectionListener, TreeSelectionListener {
	
	private JTree tree;
	private HexTable hexTable;
	private TableSorter sorter = null;

	EtherealTableModel(JTree tree, HexTable hexTable) {
		this.tree = tree;
		this.hexTable = hexTable;
	}
	
	final public static String[] columnNames = {"Num", "Time", "Original Length", "Source IP", "Destination IP", "Protocol", "Info"};
	final public static Object[] longValues = { 1000, "12:00:00.000 Jan 01, 2000 ", 65565, "255.255.255.255 ",
			"255.255.255.255 ", "Ethernet(IPv4(TCP(FTP))) ",
			"A really long explanation of the data set to pad out the column" };
	
	private ArrayList<EtherealPacket> packets = new ArrayList<EtherealPacket>();
	public ArrayList<EtherealPacket> getPackets() {
		return (ArrayList<EtherealPacket>) packets.clone();
	}
	
	EtherealPacket curPacket;
	
	public String getColumnName(int i) { return columnNames[i]; }
	public void setSorter(TableSorter s) { sorter = s; }
	
	public void addPacket(EtherealPacket p) {
		packets.add(p);
		fireTableRowsInserted(packets.size()-1, packets.size()-1);
	}
	
	public int getRowCount() {
		return packets.size();
	}

	public int getColumnCount() {
		return columnNames.length;
	}
	
	public Class<?> getColumnClass(int column) {
		/*
		switch(column) {
		case 0: return Integer.class;
		case 1: return Double.class;
		case 2: return String.class;
		case 3: return String.class;
		case 4:
			return String.class;
		case 5:
			return String.class;
		case 6: 
			return String.class; // dw: added for interface name
		}
		return Object.class;
		*/
		return longValues[column].getClass();
	}

	public synchronized Object getValueAt(int rowIndex, int columnIndex) {
		return packets.get(rowIndex).getRowData(columnIndex);
	}

	public void valueChanged(ListSelectionEvent e) {
		//if (e.getValueIsAdjusting() == true) return;
		
		
		if (e.getSource() != hexTable.getSelectionModel() && e.getSource() != hexTable.getColumnModel().getSelectionModel()) {
			int index = ((ListSelectionModel)e.getSource()).getMinSelectionIndex();
			
			
			if (sorter != null) index = sorter.modelIndex(index);
			if (index == -1) {
				tree.setModel(null);
				hexTable.setModel(new DefaultTableModel());
				return;
			}
			
			curPacket = packets.get(index);
			TreeNode rootNode = curPacket.getTree();
			TreeModel model = new DefaultTreeModel(rootNode);

			/*
			DefaultMutableTreeNode node = (DefaultMutableTreeNode) tree.getLastSelectedPathComponent();
			
			int level = 0;
			if (node != null)
				level = node.getLevel();
			*/
			
			tree.setModel(model);

			TableModel tableModel = curPacket.getTableModel();
			hexTable.setModel(tableModel);
			hexTable.clearSelection();

	 	   	Object[] hexLongValues = {"00", "00", "00", "00", "00", "00", "00", "00", "00", "00", "00", "00", "00", "00", "00", "00", "ZZZZZZZZZZZZZZZZ"};
	 	   	Ethereal.initColumnSizes(hexTable, hexLongValues);
			
		} else {

			int col = hexTable.getColumnModel().getSelectionModel().getMinSelectionIndex();
			if (col == 16) return;
			
			
			int row = hexTable.getSelectionModel().getMinSelectionIndex();
			curPacket.hexSelectionChanged(row, col, tree, hexTable);
			tree.scrollPathToVisible(tree.getSelectionPath());
			tree.revalidate();
		}

	}

	public void valueChanged(TreeSelectionEvent e) {
		EtherealTreeNode node = (EtherealTreeNode) tree.getLastSelectedPathComponent();
		if (node != null)
			curPacket.treeSelectionChanged(node, hexTable);
	}
	public void clear() {
		packets.clear();
		EtherealPacket.clear();
		fireTableRowsDeleted(0, packets.size());
	}

}
