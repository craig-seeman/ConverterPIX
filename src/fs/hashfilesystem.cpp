 /******************************************************************************
 *
 *  Project:	mbd_reader @ core
 *  File:		/fs/hashfilesystem.cxx
 *
 *  Copyright (C) 2013 - 2017, TruckersMP Team.
 *  All rights reserved.
 *
 *   This software is ditributed WITHOUT ANY WARRANTY; without even
 *   the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *   PURPOSE. See the copyright file for more information.
 *
 *****************************************************************************/

#include <prerequisites.h>

#include "hashfilesystem.h"

#include "sysfilesystem.h"
#include "file.h"
#include "hashfs_file.h"

#include <utils/string_tokenizer.h>

HashFileSystem::HashFileSystem(const String &root)
{
	m_rootFilename = root;
	m_root = getSFS()->open(root, FileSystem::read | FileSystem::binary);
	if (!m_root)
	{
		error("hashfs", root, "Unable to open root file");
		return;
	}
	readHashFS();
}

HashFileSystem::~HashFileSystem()
{
}

String HashFileSystem::root() const
{
	return m_rootFilename;
}

UniquePtr<File> HashFileSystem::open(const String &filename, FsOpenMode mode)
{
	using namespace prism;

	hashfs_entry_t *const entry = findEntry(filename);
	if (!entry)
	{
		return UniquePtr<File>();
	}

	return std::make_unique<HashFsFile>(filename, this, entry);
}

bool HashFileSystem::mkdir(const String &directory)
{
	return false;
}

bool HashFileSystem::rmdir(const String &directory)
{
	return false;
}

bool HashFileSystem::exists(const String &filename)
{
	using namespace prism;

	if (filename.empty())
	{
		return false;
	}

	hashfs_entry_t *const entry = findEntry(filename);
	if (!entry)
	{
		return false;
	}

	if (entry->m_flags & HASHFS_DIR)
	{
		return false;
	}

	return true;
}

bool HashFileSystem::dirExists(const String &dirpath)
{
	using namespace prism;

	if (dirpath.empty())
	{
		return false;
	}

	hashfs_entry_t *const entry = findEntry(dirpath.size() != 1 ? removeSlashAtEnd(dirpath) : dirpath);
	if (!entry)
	{
		return false;
	}

	if (!(entry->m_flags & HASHFS_DIR))
	{
		return false;
	}

	return true;
}

auto HashFileSystem::readDir(const String &path, bool absolutePaths, bool recursive) -> UniquePtr<List<Entry>>
{
	using namespace prism;

	if (path.empty())
	{
		error("hashfs", m_rootFilename, "readDir: Path is empty!");
		return UniquePtr<List<Entry>>();
	}

	String dirpath = path.size() != 1 ? removeSlashAtEnd(path) : path;

	hashfs_entry_t *const entry = findEntry(dirpath);
	if (!entry)
	{
		error_f("hashfs", m_rootFilename, "Failed to open dirlist entry (%s)", path);
		return UniquePtr<List<Entry>>();
	}

	if (!(entry->m_flags & HASHFS_DIR))
	{
		error_f("hashfs", m_rootFilename, "Entry is not directory");
		return UniquePtr<List<Entry>>();
	}

	HashFsFile directoryFile(path, this, entry);
	const size_t size = directoryFile.size();

	UniquePtr<char[]> buffer(new char[size + 1]);
	directoryFile.blockRead(buffer.get(), 0, size);
	buffer[size] = '\0';
	String data = buffer.get();

	auto result = std::make_unique<List<Entry>>();

	StringTokenizer tokenizer(data, "\n");
	for (String line; tokenizer.getNext(&line);)
	{
		if (line[0] == '*') // directory
		{
			String directorypath = removeSlashAtEnd(dirpath) + "/" + String(line.c_str() + 1);
			result->push_back(Entry(directorypath, true, this));
			if (recursive)
			{
				auto subdir = readDir(directorypath, absolutePaths, recursive);
				if (subdir)
				{
					result->insert(result->end(), subdir->begin(), subdir->end());
				}
			}
		}
		else
		{
			String filepath = removeSlashAtEnd(dirpath) + "/" + line.c_str();
			result->push_back(Entry(filepath, false, this));
		}
	}

	return result;
}

bool HashFileSystem::ioRead(void *const buffer, size_t bytes, size_t offset)
{
	return m_root->blockRead(buffer, offset, bytes);
}

bool HashFileSystem::readHashFS()
{
	using namespace prism;

	if (!m_root->blockRead(&m_header, 0, sizeof(hashfs_header_t)))
	{
		error("hashfs", m_rootFilename, "Failed to read header!");
		return false;
	}

	if (m_header.m_version != hashfs_header_t::SUPPORTED_VERSION)
	{
		error_f("hashfs", m_rootFilename, "Unsupported version (%u)", m_header.m_version);
		return false;
	}

	if (m_header.m_hash_method != MAKEFOURCC('C', 'I', 'T', 'Y'))
	{
		error_f("hashfs", m_rootFilename, "Unsupported hash method (%08X)", m_header.m_hash_method);
		return false;
	}

	if (m_header.m_entries_count > 200000)
	{
		error("hashfs", m_rootFilename, "Entry table size exceeds internal limits!");
		return false;
	}

	m_entries.resize(m_header.m_entries_count);
	if (!m_root->blockRead(m_entries.data(), m_header.m_start_offset, m_header.m_entries_count * sizeof(hashfs_entry_t)))
	{
		error("hasfs", m_rootFilename, "Failed to read entries!");
		return false;
	}

	return true;
}

prism::hashfs_entry_t *HashFileSystem::findEntry(String path)
{
	using namespace prism;

	if (m_header.m_salt != 0)
	{
		path = fmt::sprintf("%u%s", m_header.m_salt, (path.c_str() + 1));
	}
	else
	{
		path = path.c_str() + 1;
	}

	const u64 hash = city_hash_64(path.c_str(), path.length());

	size_t index, l = 0, r = m_entries.size();
	while (l <= r) // binary search
	{
		index = (l + r) / 2;

		if (m_entries[index].m_hash == hash)
		{
			return &m_entries[index];
		}

		if (m_entries[index].m_hash > hash)
		{
			r = index - 1;
		}
		else
		{
			l = index + 1;
		}
	}
	return nullptr;
}

/* eof */