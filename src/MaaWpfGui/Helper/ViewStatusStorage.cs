// <copyright file="ViewStatusStorage.cs" company="MaaAssistantArknights">
// MaaWpfGui - A part of the MaaCoreArknights project
// Copyright (C) 2021 MistEO and Contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY
// </copyright>

using System;
using System.IO;
using System.Reflection;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace MaaWpfGui
{
    /// <summary>
    /// 界面设置存储（读写json文件）
    /// </summary>
    public class ViewStatusStorage
    {
        private static readonly string _configFilename = Environment.CurrentDirectory + "\\config\\gui.json";
        private static readonly string _configBakFilename = Environment.CurrentDirectory + "\\config\\gui.json.bak";
        private static JObject _viewStatus = new JObject();

        /// <summary>
        /// Gets the value of a key with default value.
        /// </summary>
        /// <param name="key">The key.</param>
        /// <param name="default_value">The default value.</param>
        /// <returns>The value, or <paramref name="default_value"/> if <paramref name="key"/> is not found.</returns>
        public static string Get(string key, string default_value)
        {
            if (_viewStatus.ContainsKey(key))
            {
                return _viewStatus[key].ToString();
            }
            else
            {
                return default_value;
            }
        }

        /// <summary>
        /// Sets a key with a value.
        /// </summary>
        /// <param name="key">The key.</param>
        /// <param name="value">The value.</param>
        public static void Set(string key, string value)
        {
            _viewStatus[key] = value;
            Save();
        }

        /// <summary>
        /// Loads configuration.
        /// </summary>
        /// <returns>Whether the operation is successful.</returns>
        public static bool Load()
        {
            // 2023-1-13 配置文件迁移
            // FIXME: 之后的版本删了这段
            Directory.CreateDirectory("config");
            if (File.Exists("gui.json"))
            {
                if (File.Exists(_configFilename))
                {
                    File.Delete("gui.json");
                }
                else
                {
                    File.Move("gui.json", _configFilename);
                }
            }

            File.Delete("gui.json.bak");
            File.Delete("gui.json.bak1");
            File.Delete("gui.json.bak2");

            if (File.Exists(_configFilename))
            {
                try
                {
                    string jsonStr = File.ReadAllText(_configFilename);

                    if (jsonStr.Length <= 2 && File.Exists(_configBakFilename))
                    {
                        jsonStr = File.ReadAllText(_configBakFilename);
                        try
                        {
                            File.Copy(_configBakFilename, _configFilename, true);
                        }
                        catch (Exception e)
                        {
                            Logger.Error(e.ToString(), MethodBase.GetCurrentMethod().Name);
                        }
                    }

                    _viewStatus = (JObject)JsonConvert.DeserializeObject(jsonStr) ?? new JObject();
                }
                catch (Exception e)
                {
                    Logger.Error(e.ToString(), MethodBase.GetCurrentMethod().Name);
                    _viewStatus = new JObject();
                    return false;
                }
            }
            else
            {
                _viewStatus = new JObject();
                return false;
            }

            BakeUpDaily();
            return true;
        }

        /// <summary>
        /// Deletes configuration.
        /// </summary>
        /// <param name="key">The key.</param>
        /// <returns>Whether the operation is successful.</returns>
        public static bool Delete(string key)
        {
            try
            {
                _viewStatus.Remove(key);
                Save();
            }
            catch (Exception)
            {
                return false;
            }

            return true;
        }

        /// <summary>
        /// Saves configuration.
        /// </summary>
        /// <returns>Whether the operation is successful.</returns>
        public static bool Save()
        {
            if (_released)
            {
                return false;
            }

            try
            {
                var jsonStr = _viewStatus.ToString();
                if (jsonStr.Length > 2)
                {
                    using (StreamWriter sw = new StreamWriter(_configFilename))
                    {
                        sw.Write(jsonStr);
                    }

                    if (new FileInfo(_configFilename).Length > 2)
                    {
                        File.Copy(_configFilename, _configBakFilename, true);
                    }
                }
            }
            catch (Exception)
            {
                return false;
            }

            return true;
        }

        private static bool _released = false;

        public static void Release()
        {
            Save();
            _released = true;
        }

        /// <summary>
        /// Backs up configuration daily. (#2145)
        /// </summary>
        /// <param name="num">The number of backup files.</param>
        /// <returns>Whether the operation is successful.</returns>
        public static bool BakeUpDaily(int num = 2)
        {
            if (File.Exists(_configBakFilename) && DateTime.Now.Date != new FileInfo(_configBakFilename).LastWriteTime.Date && num > 0)
            {
                try
                {
                    for (; num > 1; num--)
                    {
                        if (File.Exists(string.Concat(_configBakFilename, num - 1)))
                        {
                            File.Copy(string.Concat(_configBakFilename, num - 1), string.Concat(_configBakFilename, num), true);
                        }
                    }

                    File.Copy(_configBakFilename, string.Concat(_configBakFilename, num), true);
                }
                catch (Exception)
                {
                    return false;
                }
            }

            return true;
        }
    }
}
