package org.netxms.base;

import java.io.*;
import java.net.*;
import java.util.Arrays;

/**
 * @author victor
 */
public class NXCPVariable
{
	public static final int TYPE_INTEGER = 0;
	public static final int TYPE_STRING = 1;
	public static final int TYPE_INT64 = 2;
	public static final int TYPE_INT16 = 3;
	public static final int TYPE_BINARY = 4;
	public static final int TYPE_FLOAT = 5;

	private long variableId;
	private int variableType;

	private Long integerValue;
	private Double realValue;
	private String stringValue;
	private byte[] binaryValue;

	
	//
	// Overriden toString()
	//
	
	@Override
	public String toString()
	{
		StringBuilder result = new StringBuilder();
		String NEW_LINE = System.getProperty("line.separator");

		result.append(this.getClass().getName() + " Object {" + NEW_LINE);
		result.append(" variableId = " + Long.toString(variableId) + NEW_LINE);
		result.append(" variableType = " + variableType + NEW_LINE);
		result.append(" integerValue = " + integerValue + NEW_LINE);
		result.append(" realValue = " + realValue + NEW_LINE);
		result.append(" stringValue = " + stringValue + NEW_LINE);
		result.append(" binaryValue = " + Arrays.toString(binaryValue) + NEW_LINE);
		result.append("}");

		return result.toString();
	}
	
	
	//
	// Set string value and numeric values if possible
	//
	
	private void setStringValue(String value)
	{
		stringValue = value;
		try
		{
			integerValue = Long.parseLong(stringValue);
		}
		catch (NumberFormatException e)
		{
			integerValue = 0L;
		}
		try
		{
			realValue = Double.parseDouble(stringValue);
		}
		catch (NumberFormatException e)
		{
			realValue = (double)0;
		}
	}


	/**
	 * @param varId
	 * @param varType
	 * @param value
	 */
	
	public NXCPVariable(final long varId, final int varType, final Long value)
	{
		variableId = varId;
		variableType = varType;
		integerValue = value;
		stringValue = integerValue.toString();
		realValue = integerValue.doubleValue();
	}

	/**
	 * @param varId
	 * @param value
	 */
	public NXCPVariable(final long varId, final String value)
	{
		variableId = varId;
		variableType = TYPE_STRING;
		setStringValue(value);
	}

	/**
	 * @param varId
	 * @param value
	 */
	public NXCPVariable(final long varId, final Double value)
	{
		variableId = varId;
		variableType = TYPE_FLOAT;
		realValue = value;
		stringValue = value.toString();
		integerValue = realValue.longValue();
	}

	/**
	 * @param varId
	 * @param value
	 */
	public NXCPVariable(final long varId, final byte[] value)
	{
		variableId = varId;
		variableType = TYPE_BINARY;
		binaryValue = value;
		stringValue = "";
		integerValue = (long)0;
		realValue = (double)0;
	}

	/**
	 * Create NXCPVariable from NXCP message data field
	 *
	 * @param nxcpDataField
	 * @throws java.io.IOException
	 */
	public NXCPVariable(final byte[] nxcpDataField) throws IOException
	{
		NXCPDataInputStream in = new NXCPDataInputStream(nxcpDataField);

		variableId = (long)in.readUnsignedInt();
		variableType = in.readUnsignedByte();
		in.skipBytes(1);	// Skip padding
		if (variableType == TYPE_INT16)
		{
			integerValue = (long)in.readUnsignedShort();
			realValue = integerValue.doubleValue();
			stringValue = integerValue.toString();
		}
		else
		{
			in.skipBytes(2);
			switch(variableType)
			{
				case TYPE_INTEGER:
					integerValue = (long)in.readInt();
					realValue = integerValue.doubleValue();
					stringValue = integerValue.toString();
					break;
				case TYPE_INT64:
					integerValue = in.readLong();
					realValue = integerValue.doubleValue();
					stringValue = integerValue.toString();
					break;
				case TYPE_FLOAT:
					realValue = in.readDouble();
					integerValue = realValue.longValue();
					stringValue = realValue.toString();
					break;
				case TYPE_STRING:
					int len = in.readInt() / 2;
					StringBuilder sb = new StringBuilder(len);
					while (len > 0)
					{
						sb.append(in.readChar());
						len--;
					}
					setStringValue(sb.toString());
					break;
				case TYPE_BINARY:
					binaryValue = new byte[in.readInt()];
					in.readFully(binaryValue);
					break;
			}
		}
	}


	public Long getAsInteger()
	{
		return integerValue;
	}

	public Double getAsReal()
	{
		return realValue;
	}

	public String getAsString()
	{
		return stringValue;
	}

	public byte[] getAsBinary()
	{
		return binaryValue;
	}

	public InetAddress getAsInetAddress()
	{
		final byte[] addr = new byte[4];
		final long intVal = integerValue.longValue();
		InetAddress inetAddr;
		
		addr[0] =  (byte)((intVal & 0xFF000000) >> 24);
		addr[1] =  (byte)((intVal & 0x00FF0000) >> 16);
		addr[2] =  (byte)((intVal & 0x0000FF00) >> 8);
		addr[3] =  (byte)(intVal & 0x000000FF);
		
		try
		{
			inetAddr = InetAddress.getByAddress(addr);
		}
		catch(UnknownHostException e)
		{
			inetAddr = null;
		}
		return inetAddr;
	}

	/**
	 * @return the variableId
	 */
	public long getVariableId()
	{
		return variableId;
	}

	/**
	 * @param variableId the variableId to set
	 */
	public void setVariableId(final long variableId)
	{
		this.variableId = variableId;
	}

	/**
	 * @return the variableType
	 */
	public int getVariableType()
	{
		return variableType;
	}


	/**
	 * Create NXCP DF structure
	 *
	 * @return
	 */
	private int calculateBinarySize()
	{
		final int size;

		switch(variableType)
		{
			case TYPE_INTEGER:
				size = 12;
				break;
			case TYPE_INT64:
			case TYPE_FLOAT:
				size = 16;
				break;
			case TYPE_INT16:
				size = 8;
				break;
			case TYPE_STRING:
				size = stringValue.length() * 2 + 12;
				break;
			case TYPE_BINARY:
				size = binaryValue.length + 12;
				break;
			default:
				size = 8;
				break;
		}
		return size;
	}

	public byte[] createNXCPDataField() throws IOException
	{
		final ByteArrayOutputStream byteStream = new ByteArrayOutputStream(calculateBinarySize());
		final DataOutputStream out = new DataOutputStream(byteStream);

		out.writeInt(Long.valueOf(variableId).intValue());
		out.writeByte(Long.valueOf(variableType).byteValue());
		out.writeByte(0);		// Padding
		if (variableType == TYPE_INT16)
		{
			out.writeShort(integerValue.shortValue());
		}
		else
		{
			out.writeShort(0);	// Padding
			switch(variableType)
			{
				case TYPE_INTEGER:
					out.writeInt(integerValue.intValue());
					break;
				case TYPE_INT64:
					out.writeLong(integerValue);
					break;
				case TYPE_FLOAT:
					out.writeDouble(realValue);
					break;
				case TYPE_STRING:
					out.writeInt(stringValue.length() * 2);
					out.writeChars(stringValue);
					break;
				case TYPE_BINARY:
					out.writeInt(binaryValue.length);
					out.write(binaryValue);
					break;
			}
		}

		// Align to 8-bytes boundary
		final int rem = byteStream.size() % 8;
		if (rem != 0)
		{
			out.write(new byte[8 - rem]);
		}

		return byteStream.toByteArray();
	}
}
