        .text
        LD *AR2+, A
        LD *+AR2, A
        LD *+AR2(1), A
        STL A, *+AR2
        ANDM #0ffh, *+AR2
        ORM #1, *+AR2
        XORM #1, *+AR2
        ADDM #1, *+AR2
        .end
