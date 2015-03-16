#ifndef ED_IO_JSON_WRITER_H_
#define ED_IO_JSON_WRITER_H_

#include "ed/io/writer.h"

#include <sstream>

namespace ed
{

namespace io
{

class JSONWriter : Writer
{

public:

    JSONWriter(std::ostream& out) : Writer(out), add_comma_(false)
    {
        out << "{";
    }

    ~JSONWriter() {}

    void writeGroup(const std::string& name)
    {
        if (add_comma_)
            out_ << ",";

        out_ << "\"" << name << "\":{";
        type_stack_.push_back('g');
        add_comma_ = false;
    }

    void endGroup()
    {
        out_ << "}";
        type_stack_.pop_back();
        add_comma_ = true;
    }

    void writeValue(const std::string& key, float f)
    {
        if (add_comma_)
            out_ << ",";

        out_ << "\"" << key << "\":\"" << f << "\"";
        add_comma_ = true;
    }

    void writeValue(const std::string& key, int i)
    {
        if (add_comma_)
            out_ << ",";

        out_ << "\"" << key << "\":\"" << i << "\"";
        add_comma_ = true;
    }

    void writeValue(const std::string& key, const std::string& s)
    {
        if (add_comma_)
            out_ << ",";

        out_ << "\"" << key << "\":\"" << s << "\"";
        add_comma_ = true;
    }

    void writeValue(const std::string& key, const float* fs, std::size_t size)
    {
        if (add_comma_)
            out_ << ",";

        out_ << "\"" << key << "\":[";

        if (size > 0)
        {
            out_ << fs[0];
            for(unsigned int i = 1; i < size; ++i)
                out_ << "," << fs[i];
        }
        out_ << "]";
    }

    void writeValue(const std::string& key, const int* is, std::size_t size)
    {
        if (add_comma_)
            out_ << ",";

        out_ << "\"" << key << "\":[";

        if (size > 0)
        {
            out_ << is[0];
            for(unsigned int i = 1; i < size; ++i)
                out_ << "," << is[i];
        }
        out_ << "]";
    }

    void writeValue(const std::string& key, const std::string* ss, std::size_t size)
    {
        if (add_comma_)
            out_ << ",";

        out_ << "\"" << key << "\":[";

        if (size > 0)
        {
            out_ << "\"" << ss[0] << "\"";
            for(unsigned int i = 1; i < size; ++i)
                out_ << "\"" << ss[i] << "\"";
        }
        out_ << "]";
    }

    void writeArray(const std::string& key)
    {
        if (add_comma_)
            out_ << ",";

        out_ << "[";
        type_stack_.push_back('a');
        add_comma_ = false;
    }

    void addArrayItem() {}
    void endArrayItem() {}

    void endArray()
    {
        out_ << "]";
        type_stack_.pop_back();
        add_comma_ = true;
    }

    void finish()
    {
        while(!type_stack_.empty())
        {
            char t = type_stack_.back();
            if (t == 'g')
                endGroup();
            else if (t == 'a')
                endArray();
        }
        out_ << "}";
    }

private:

    bool add_comma_;
    std::vector<char> type_stack_;

};

}

} // end namespace era

#endif
