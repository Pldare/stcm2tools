// Will o' Wisp DS - http://www.otomate.jp/will_psp_ds/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace stcm2insert
{
    class Program
    {
        struct Label
        {
            public string exportName;
            public int exportOffset;

            public Label(string _exportName, int _exportOffset)
            {
                exportName = _exportName;
                exportOffset = _exportOffset;
            }
        }

        static Dictionary<string, string> strings = new Dictionary<string, string>();

        static void Main(string[] args)
        {
            ReadStringFile(args[1]);
            ReadBytecodeFile(args[0], args[2]);
        }

        static void ReadStringFile(string inputFile)
        {
            StreamReader textFile = new StreamReader(File.OpenRead(inputFile), Encoding.GetEncoding(932));

            while (!textFile.EndOfStream)
            {
                string input = textFile.ReadLine().TrimStart().TrimEnd('\r').TrimEnd('\n');

                if (!input.StartsWith("<"))
                    continue;
                if (input.Contains("//"))
                    input = input.Remove(input.IndexOf("//"));

                string id = input.Substring(1, input.IndexOf('>') - 1);
                input = input.Remove(0, input.IndexOf('>') + 1).TrimStart();

                if (strings.ContainsKey(id))
                {
                    Console.WriteLine("Found duplicate string id: {0}", id);
                    Environment.Exit(1);
                }

                strings.Add(id, input);
            }

            textFile.Close();
        }

        static void ReadBytecodeFile(string inputFile, string outputName)
        {
            StreamReader bytecodeFile = new StreamReader(File.OpenRead(inputFile), Encoding.GetEncoding(932));
            MemoryStream outputData = new MemoryStream();
            BinaryWriter output = new BinaryWriter(outputData);

            List<Label> exports = new List<Label>();
            List<Label> labels = new List<Label>();
            List<Label> postFix = new List<Label>();

            output.Write("STCM2 File Make By Minku 07.0\0\0\0".ToCharArray());
            output.Write(new String('\0', 0x30).ToCharArray());

            while (!bytecodeFile.EndOfStream)
            {
                string input = bytecodeFile.ReadLine().Trim();

                while (input != "")
                {
                    if (input.StartsWith("@@@"))
                    {
                        // export
                        // for some reason this had problems when using just a substring(3, indexof) so i chaned all of these
                        string exportName = input.TrimStart('@');
                        exportName = exportName.Substring(0, exportName.IndexOf(':'));
                        input = input.Remove(0, input.IndexOf(':') + 1);

                        exports.Add(new Label(exportName, Convert.ToInt32(output.BaseStream.Position)));
                    }
                    else if (input.StartsWith("@@"))
                    {
                        // label set
                        string labelName = input.TrimStart('@');
                        labelName = labelName.Substring(0, labelName.IndexOf(':'));
                        input = input.Remove(0, input.IndexOf(':') + 1);

                        labels.Add(new Label(labelName, Convert.ToInt32(output.BaseStream.Position)));
                    }
                    else if (input.StartsWith("@"))
                    {
                        // label reference
                        string labelName = input.TrimStart('@');
                        labelName = labelName.Substring(0, labelName.IndexOf('@'));
                        input = input.Remove(0, input.IndexOf('@', 1) + 1);

                        postFix.Add(new Label(labelName, Convert.ToInt32(output.BaseStream.Position)));
                        output.Write((int)0);
                    }
                    else if (input.StartsWith("["))
                    {
                        // add new section
                        string sectionName = input.Substring(1, input.IndexOf(']') - 1);
                        input = input.Remove(0, input.IndexOf(']') + 1);

                        output.Write(sectionName.ToCharArray());
                        output.Write('\0');
                    }
                    else if (input.StartsWith("{"))
                    {
                        // normal command
                        string data = input.Substring(1, input.IndexOf('}') - 1);
                        input = input.Remove(0, input.IndexOf('}') + 1);

                        if (data.Contains(' '))
                        {
                            // multiple datas contained in one block
                            string[] datas = data.Split(' ');

                            foreach (string dataStr in datas)
                            {
                                uint num = UInt32.Parse(dataStr, System.Globalization.NumberStyles.HexNumber);

                                if (dataStr.Length == 2)
                                    output.Write((byte)num);
                                else if (dataStr.Length == 4)
                                    output.Write((ushort)num);
                                else if(dataStr.Length == 8)
                                    output.Write((uint)num);
                            }
                        }
                        else
                        {
                            uint num = UInt32.Parse(data, System.Globalization.NumberStyles.HexNumber);

                            if (data.Length == 2)
                                output.Write((byte)num);
                            else if (data.Length == 4)
                                output.Write((ushort)num);
                            else if (data.Length == 8)
                                output.Write((uint)num);
                        }
                    }
                    else if (input.StartsWith("<"))
                    {
                        // string reference
                        string id = input.Substring(1, input.IndexOf('>') - 1);
                        input = input.Remove(0, input.IndexOf('>') + 1);

                        if (strings.ContainsKey(id))
                        {
                            int stringSize = Encoding.GetEncoding(932).GetByteCount(strings[id]) + 1;
                            int sizeOffset = Convert.ToInt32(output.BaseStream.Position) - 4;

                            output.Write(Encoding.GetEncoding(932).GetBytes(strings[id]));
                            output.Write('\0');


                            //output.Seek(sizeOffset, SeekOrigin.Begin);
                            //stringSize = Convert.ToInt32(outputData.ReadByte()); 
                            //int extraLen = stringSize - Encoding.GetEncoding(932).GetByteCount(strings[id]);

                            int padLen = 4;
                            int extraLen = padLen - ((int)output.BaseStream.Length % padLen);
                            stringSize += extraLen;

                            output.Seek(sizeOffset, SeekOrigin.Begin);
                            output.Write(stringSize);
                            output.Seek(0, SeekOrigin.End);

                            //Console.WriteLine(stringSize + " " + extraLen + " " + strings[id]);

                            if (extraLen != 0)
                                output.Write(new String('\0', extraLen).ToCharArray());
                        }
                        else
                        {
                            Console.WriteLine("Unknown string ID: {0}", id);
                            Environment.Exit(1);
                        }
                    }
                    else
                    {
                        // unknown
                    }

                    input = input.Trim();
                }
            }

            output.Flush();

            foreach (Label fix in postFix)
            {
                //Console.WriteLine(fix.exportName + " " + fix.exportOffset);

                output.Seek(fix.exportOffset, SeekOrigin.Begin);

                int newOffset = 0;
                foreach(Label label in labels)
                {
                    if (label.exportName == fix.exportName)
                    {
                        newOffset = label.exportOffset;
                        break;
                    }
                }

                output.Write(newOffset);
            }

            output.Seek(0, SeekOrigin.End);
            output.Write("EXPORT_DATA".ToCharArray());
            output.Write('\0');

            int exportDataOffset = Convert.ToInt32(output.BaseStream.Length);
            output.Seek(0x20, SeekOrigin.Begin);
            output.Write(exportDataOffset);
            output.Write(exports.Count);
            output.Seek(0, SeekOrigin.End);

            foreach (Label export in exports)
            {
                output.Write((int)0);
                output.Write(export.exportName.ToCharArray());
                output.Write(new String('\0', 0x20 - (export.exportName.Length % 0x20)).ToCharArray());
                output.Write(export.exportOffset);
            }

            output.Write(new String('\0', 16 - ((int)output.BaseStream.Length % 16)).ToCharArray());
            output.Flush();

            using (var outputFile = File.Open(outputName, FileMode.Create))
            {
                Console.WriteLine("Output size: {0} bytes", outputData.Length);
                outputData.WriteTo(outputFile);
                outputFile.Flush();
                outputFile.Close();
            }

            bytecodeFile.Close();
        }
    }
}
