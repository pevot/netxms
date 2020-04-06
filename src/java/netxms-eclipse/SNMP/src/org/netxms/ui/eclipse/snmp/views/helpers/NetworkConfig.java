package org.netxms.ui.eclipse.snmp.views.helpers;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import org.netxms.client.NXCException;
import org.netxms.client.NXCSession;
import org.netxms.client.snmp.SnmpUsmCredential;

public class NetworkConfig
{
   // Global SNMP config flag
   public static int NETWORK_CONFIG_GLOBAL = -1;
   public static int NETWORK_CONFIG_ALL = -2;
   
   //Configuration type flags
   public static int COMUNITIES  = 0x01;
   public static int USM         = 0x02;
   public static int PORTS       = 0x04;
   public static int SECRET      = 0x08;
   public static int ALL_CONFIGS = 0x0F; 
   
   private Map<Integer, List<String>> communities = new HashMap<Integer, List<String>>();
   private Map<Integer, List<SnmpUsmCredential>> usmCredentials = new HashMap<Integer, List<SnmpUsmCredential>>();
   private Map<Integer, List<Short>> ports = new HashMap<Integer, List<Short>>();
   private Map<Integer, List<String>> sharedSecrets = new HashMap<Integer, List<String>>();
   private NXCSession session;
   private Map<Integer, Integer> changedConfig = new HashMap<Integer, Integer>();

   /**
    * Create object
    */
   public NetworkConfig(NXCSession session)
   {
      this.session = session;
   }
   
   /**
    * Load exact configuration part 
    * 
    * @param configId id of configuration objects
    */
   public void load(int configId, long zoneUIN) throws NXCException, IOException
   {
      if((configId & COMUNITIES) > 0)
      {
         if(NETWORK_CONFIG_ALL == zoneUIN)
            communities.clear();
         communities.putAll(session.getSnmpCommunities(zoneUIN));      
      }
      
      if((configId & USM) > 0)
      {
         if(NETWORK_CONFIG_ALL == zoneUIN)
            usmCredentials.clear();
         usmCredentials.putAll(session.getSnmpUsmCredentials(zoneUIN));      
      }
      
      if((configId & PORTS) > 0)
      {
         if(NETWORK_CONFIG_ALL == zoneUIN)
            ports.clear();
         ports.putAll(session.getSNMPPors(zoneUIN));      
      }
      
      if((configId & SECRET) > 0)
      {
         if(NETWORK_CONFIG_ALL == zoneUIN)
            sharedSecrets.clear();
         sharedSecrets.putAll(session.getShredSecrets(zoneUIN));      
      }
   }
   
   public boolean isChanged(int configId, long zoneUIN)
   {
      return (changedConfig.getOrDefault((int)zoneUIN, 0) & configId) > 0;
   }
   
   /**
    * Save SNMP configuration on server. This method calls communication
    * API directly, so it should not be called from UI thread.
    * 
    * @params session communication session to use
    * @throws IOException if socket I/O error occurs
    * @throws NXCException if NetXMS server returns an error or operation was timed out
    */
   public void save(NXCSession session) throws NXCException, IOException
   {
      for (Entry<Integer, Integer> value : changedConfig.entrySet())
         if((value.getValue() & COMUNITIES) > 0)
         {
            session.updateSnmpCommunities(communities.get(value.getKey()), (int)value.getKey());   
         }
      
      for (Entry<Integer, Integer> value : changedConfig.entrySet())
         if((value.getValue() & USM) > 0)
         {
            session.updateSnmpUsmCredentials(usmCredentials.get(value.getKey()), (int)value.getKey());    
         }
      
      for (Entry<Integer, Integer> value : changedConfig.entrySet())
         if((value.getValue() & PORTS) > 0)
         {
            session.updateSNMPPorts(ports.get(value.getKey()), (int)value.getKey());
         }
      
      for (Entry<Integer, Integer> value : changedConfig.entrySet())
         if((value.getValue() & SECRET) > 0)
         {
            session.updateSharedSecrets(sharedSecrets.get(value.getKey()), (int)value.getKey());    
         }
      
      changedConfig.clear();
   }
   
   /**
    * Provide id of updated configuration
    * 
    * @param id
    */
   public void setConfigUpdate(long zoneUIN, int id)
   {
      changedConfig.put((int)zoneUIN, changedConfig.getOrDefault((int)zoneUIN, 0) | id);
   }
   
   /**
    * @return the communities
    */
   public List<String> getCommunities(long zoneUIN)
   {
      if (communities.containsKey((int)zoneUIN))
         return communities.get((int)zoneUIN);
      else
         return new ArrayList<String>();
   }

   /**
    * @param communities the communities to set
    */
   public void addCommunityString(String communityString, long zoneUIN)
   {
      if (this.communities.containsKey((int)zoneUIN))
         this.communities.get((int)zoneUIN).add(communityString);
      else
      {
         List<String> list = new ArrayList<String>();
         list.add(communityString);
         this.communities.put((int)zoneUIN, list);
      }
   }
   
   /**
    * @return the ports
    */
   public List<Short> getPorts(long zoneUIN)
   {
      if (ports.containsKey((int)zoneUIN))
         return ports.get((int)zoneUIN);
      else
         return new ArrayList<Short>();
   }
   
   /**
    * @param communities the communities to set
    */
   public void addPort(Short port, long zoneUIN)
   {
      if (this.ports.containsKey((int)zoneUIN))
         this.ports.get((int)zoneUIN).add(port);
      else
      {
         List<Short> list = new ArrayList<Short>();
         list.add(port);
         this.ports.put((int)zoneUIN, list);
      }
   }

   /**
    * @return the usmCredentials
    */
   public List<SnmpUsmCredential> getUsmCredentials(long zoneUIN)
   {
      if (usmCredentials.containsKey((int)zoneUIN))
         return usmCredentials.get((int)zoneUIN);
      else
         return new ArrayList<SnmpUsmCredential>();
   }

   /**
    * @param usmCredentials the usmCredentials to set
    */
   public void addUsmCredentials(SnmpUsmCredential credential, long zoneUIN)
   {
      if (usmCredentials.containsKey((int)zoneUIN))
         usmCredentials.get((int)zoneUIN).add(credential);
      else
      {
         List<SnmpUsmCredential> list = new ArrayList<SnmpUsmCredential>();
         list.add(credential);
         usmCredentials.put((int)zoneUIN, list);
      }
   } 
   
   /**
    * @return the shared secrets
    */
   public List<String> getSharedSecrets(long zoneUIN)
   {
      if (sharedSecrets.containsKey((int)zoneUIN))
         return sharedSecrets.get((int)zoneUIN);
      else
         return new ArrayList<String>();
   }

   /**
    * @param sharedSecret the shared secret to set
    * @param zoneUIN zone UIN
    */
   public void addSharedSecret(String sharedSecret, long zoneUIN)
   {
      if (this.sharedSecrets.containsKey((int)zoneUIN))
         this.sharedSecrets.get((int)zoneUIN).add(sharedSecret);
      else
      {
         List<String> list = new ArrayList<String>();
         list.add(sharedSecret);
         this.sharedSecrets.put((int)zoneUIN, list);
      }
   }
}
