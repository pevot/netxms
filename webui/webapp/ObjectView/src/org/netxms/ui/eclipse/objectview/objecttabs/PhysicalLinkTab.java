/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2019 Raden Solutions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.ui.eclipse.objectview.objecttabs;

import org.eclipse.core.commands.Command;
import org.eclipse.core.commands.State;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.dialogs.IDialogSettings;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.layout.FormAttachment;
import org.eclipse.swt.layout.FormData;
import org.eclipse.swt.layout.FormLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.commands.ICommandService;
import org.netxms.client.objects.AbstractObject;
import org.netxms.client.objects.Interface;
import org.netxms.client.objects.Node;
import org.netxms.client.objects.Rack;
import org.netxms.ui.eclipse.objectview.Activator;
import org.netxms.ui.eclipse.objectview.widgets.PhysicalLinkWidget;
import org.netxms.ui.eclipse.tools.VisibilityValidator;

/**
 * Physical link tab for Racks and Nodes
 */
public class PhysicalLinkTab extends ObjectTab
{
   private PhysicalLinkWidget linkWidget;
   private boolean initShowFilter = false;

   @Override
   protected void createTabContent(Composite parent)
   { 
      final IDialogSettings settings = Activator.getDefault().getDialogSettings();
      initShowFilter = safeCast(settings.get("PhysicalLinkTab.showFilter"), settings.getBoolean("PhysicalLinkTab.showFilter"), initShowFilter);
      
      FormLayout formLayout = new FormLayout();
      parent.setLayout(formLayout);

      linkWidget = new PhysicalLinkWidget(getViewPart(), parent, SWT.NONE, getObject() == null ? -1 : getObject().getObjectId(), 
            0, initShowFilter, new VisibilityValidator() {  
         @Override
         public boolean isVisible()
         {
            return isActive();
         }
      }); 
      FormData fd = new FormData();
      fd.left = new FormAttachment(0, 0);
      fd.top = new FormAttachment(0, 0);
      fd.right = new FormAttachment(100, 0);
      fd.bottom = new FormAttachment(100, 0);
      linkWidget.setLayoutData(fd);       
      
      linkWidget.addDisposeListener(new DisposeListener() {

         @Override
         public void widgetDisposed(DisposeEvent e)
         {
            settings.put("PhysicalLinkTab.showFilter", linkWidget.isFilterEnabled());
         }
         
      });  
      
      linkWidget.setFilterCloseAction(new Action() {
         @Override
         public void run()
         {
            linkWidget.enableFilter(false);
            ICommandService service = (ICommandService)PlatformUI.getWorkbench().getService(ICommandService.class);
            Command command = service.getCommand("org.netxms.ui.eclipse.objectview.commands.show_physical_link_filter"); //$NON-NLS-1$
            State state = command.getState("org.netxms.ui.eclipse.objectview.commands.show_physical_link_filter.state"); //$NON-NLS-1$
            state.setValue(false);
            service.refreshElements(command.getId(), null);
         }
      });
   }
   
   /**
    * @param b
    * @param defval
    * @return
    */
   private static boolean safeCast(String s, boolean b, boolean defval)
   {
      return (s != null) ? b : defval;
   }

   @Override
   public void objectChanged(AbstractObject object)
   {
      linkWidget.setSourceObject(getObject() != null ? getObject().getObjectId(): -1, 0);
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.objectview.objecttabs.ObjectTab#refresh()
    */
   @Override
   public void refresh()
   {
      linkWidget.refresh();
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.objectview.objecttabs.ObjectTab#showForObject(org.netxms.client.objects.AbstractObject)
    */
   @Override
   public boolean showForObject(AbstractObject object)
   {
      return object instanceof Node || object instanceof Rack || object instanceof Interface;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.objectview.objecttabs.ObjectTab#selected()
    */
   @Override
   public void selected()
   {
      super.selected();
      refresh();
   }

   /**
    * Enable filter from command
    * 
    * @param isChecked if filter shuld be enabled
    */
   public void setFilterEnabled(boolean isChecked)
   {
      linkWidget.enableFilter(isChecked);
   }
}
