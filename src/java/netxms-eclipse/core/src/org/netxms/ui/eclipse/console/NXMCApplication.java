package org.netxms.ui.eclipse.console;

import java.util.Locale;
import org.eclipse.core.runtime.Platform;
import org.eclipse.equinox.app.IApplication;
import org.eclipse.equinox.app.IApplicationContext;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.PlatformUI;

/**
 * This class controls all aspects of the application's execution
 */
public class NXMCApplication implements IApplication
{
	/* (non-Javadoc)
	 * @see org.eclipse.equinox.app.IApplication#start(org.eclipse.equinox.app.IApplicationContext)
	 */
	public Object start(IApplicationContext context) throws Exception
	{
		final String locale = Platform.getPreferencesService().getString("org.netxms.ui.eclipse.console", "NL", "en", null); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
		Locale.setDefault(new Locale(locale));
		System.setProperty("osgi.nl", locale); //$NON-NLS-1$

		Display display = PlatformUI.createDisplay();
		try
		{
			int returnCode = PlatformUI.createAndRunWorkbench(display, new NXMCWorkbenchAdvisor());
			if (returnCode == PlatformUI.RETURN_RESTART)
				return IApplication.EXIT_RESTART;
			else
				return IApplication.EXIT_OK;
		} 
		finally
		{
			display.dispose();
		}
	}

	/* (non-Javadoc)
	 * @see org.eclipse.equinox.app.IApplication#stop()
	 */
	public void stop() 
	{
		final IWorkbench workbench = PlatformUI.getWorkbench();
		if (workbench == null)
			return;
		final Display display = workbench.getDisplay();
		display.syncExec(new Runnable() 
		{
			public void run() 
			{
				if (!display.isDisposed())
					workbench.close();
			}
		});
	}
}
