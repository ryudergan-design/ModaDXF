using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        Application.EnableVisualStyles();

        string tempDir = Path.Combine(Path.GetTempPath(), "ModaDXF-Setup-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(tempDir);
            string msiPath = Path.Combine(tempDir, "ModaDXF-Native.msi");
            ExtractEmbeddedMsi(msiPath);

            string arguments = "/i \"" + msiPath + "\"";
            if (args != null && args.Length > 0)
            {
                arguments += " " + QuoteArguments(args);
            }

            ProcessStartInfo startInfo = new ProcessStartInfo("msiexec.exe", arguments);
            startInfo.UseShellExecute = false;

            using (Process process = Process.Start(startInfo))
            {
                process.WaitForExit();
                return process.ExitCode;
            }
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                "Não foi possível iniciar o instalador do Moda DXF.\n\n" + ex.Message,
                "Moda DXF Setup",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
            return 1;
        }
        finally
        {
            try
            {
                if (Directory.Exists(tempDir))
                {
                    Directory.Delete(tempDir, true);
                }
            }
            catch
            {
            }
        }
    }

    private static void ExtractEmbeddedMsi(string targetPath)
    {
        Assembly assembly = Assembly.GetExecutingAssembly();
        using (Stream input = assembly.GetManifestResourceStream("ModaDXFNativeMsi"))
        {
            if (input == null)
            {
                throw new InvalidOperationException("MSI interno não encontrado.");
            }

            using (FileStream output = File.Create(targetPath))
            {
                input.CopyTo(output);
            }
        }
    }

    private static string QuoteArguments(string[] args)
    {
        string[] quoted = new string[args.Length];
        for (int i = 0; i < args.Length; i++)
        {
            quoted[i] = QuoteArgument(args[i]);
        }
        return string.Join(" ", quoted);
    }

    private static string QuoteArgument(string value)
    {
        if (string.IsNullOrEmpty(value))
        {
            return "\"\"";
        }

        if (value.IndexOfAny(new[] { ' ', '\t', '"' }) < 0)
        {
            return value;
        }

        return "\"" + value.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
    }
}
