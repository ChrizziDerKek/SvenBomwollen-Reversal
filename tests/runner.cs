using System.Diagnostics;

namespace test_runner
{
    internal class Program
    {
        static void Main(string[] args)
        {
            string file = "current.txt";
            if (!File.Exists(file))
            {
                File.Create(file).Close();
                File.WriteAllText(file, "000");
            }
            string id = File.ReadAllText(file);
            int next = string.IsNullOrEmpty(id.TrimStart('0')) ? 1 : (int.Parse(id.TrimStart('0')) + 1);
            string target = "XS_RX" + id + ".pak";
            id = next <= 9 ? "00" + next : next <= 99 ? "0" + next : "" + next;
            File.WriteAllText(file, id);
            File.Copy("tests/" + target, target);
            Console.WriteLine("Testing " + target);
            File.Move(target, "XS.pak", true);
            Thread.Sleep(10);
            Process.Start("cmd.exe", "/K start .\\Sven2.exe");
            Environment.Exit(0);
        }
    }
}