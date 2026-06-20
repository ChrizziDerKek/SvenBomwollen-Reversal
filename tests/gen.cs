namespace testgen
{
    internal class Program
    {
        static void Main(string[] args)
        {
            int c = 0;
            string cmd = "svenstatictool clear-tile-rect tutorial01v01.lvl cal_empty.lvl 0 0 25 15\n";
            string table = "";
            for (int y = 0; y < 15; y++)
            {
                for (int x = 0; x < 25; x++)
                {
                    string id = c <= 9 ? "00" + c : c <= 99 ? "0" + c : "" + c;
                    c++;
                    cmd += string.Format("svenstatictool paint-tile-type cal_empty.lvl cal_RX{0}.lvl {1} {2} 1 1 1\n", id, x, y);
                    cmd += string.Format("mudgetool patch-file XS.pak XS/levels/tutorial01v01.lvl cal_RX{0}.lvl XS_RX{0}.pak\n", id);
                    table += string.Format("{0} (X={1};Y={2}): \n", id, x, y);
                }
            }
            File.WriteAllText("run_single_tile_test.bat", cmd);
            File.WriteAllText("table.txt", table);
        }
    }
}
