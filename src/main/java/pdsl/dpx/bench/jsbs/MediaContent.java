package pdsl.dpx.bench.jsbs;

import java.util.ArrayList;
import java.util.List;
import java.io.Serializable;

public class MediaContent implements Serializable {
    public Media media;
    public List<Image> images;

    public static MediaContent BenchCase() {
        //@formatter:off
        // {
        // 	"media": {
        // 		"uri": "http://javaone.com/keynote.mpglkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj",
        // 		"title": "Javaone Keynotelkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj",
        // 		"width": 640,
        // 		"height": 480,
        // 		"format": "video/mpg4lkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj",
        // 		"duration": 18000000,   // half hour in milliseconds
        // 		"size": 58982400,       // bitrate * duration in seconds / 8 bits per byte
        // 		"bitrate": 262144,      // 256k
        // 		"persons": ["Bill Gateslkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj", "Steve Jobslkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj"],
        // 		"player": "JAVA",
        // 		"copyright": null
        // 	},

        // 	"images": [
        // 		{
        // 			"uri": "http://javaone.com/keynote_large.jpglkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj",
        // 			"title": "Javaone Keynotelkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj",
        // 			"width": 1024,
        // 			"height": 768,
        // 			"size": "LARGE"
        // 		},
        // 		{
        // 			"uri": "http://javaone.com/keynote_small.jpglkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj",
        // 			"title": "Javaone Keynotelkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj",
        // 			"width": 320,
        // 			"height": 240,
        // 			"size": "SMALL"
        // 		}
        // 	]
        // }
        Media m = new Media();
        m.uri = "http://javaone.com/keynote.你好mpglkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj";
        m.title = "Javaone Keynotelkajldfjlsk你好ajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj";
        m.width = 640;
        m.height = 480;
        m.format = "video/mpg4lkajldfjlskajdfl你好kjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj";
        m.duration = 18000000;
        m.size = 58982400;
        m.bitrate = 262144;
        m.persons = new ArrayList<String>(2);
        m.persons.add("Bill Gateslkajldfjlskajd你好flkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj");
        m.persons.add("Steve Jobslkajldfjlskajdf你好lkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj");
        m.player = Media.Player.JAVA;
        m.copyright = "aaaabbbb你好吗好好好dfjsdklfjaskl";
        Image i1 = new Image();
        i1.uri = "http://javaone.com/keynote_larg你好e.jpglkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj";
        i1.title = "Javaone Keynotelkajldfjlskajdf你好lkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj";
        i1.width = 1024;
        i1.height = 768;
        i1.size = Image.Size.LARGE;
        Image i2 = new Image();
        i2.uri = "http://javaone.com/keynote_small.你好jpglkajldfjlskajdflkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj";
        i2.title = "Javaone Keynotelkajldfjlskajd你好flkjslfjdslfjldjfljsdfljsdlfjsljfldjfldjals;djfasldjf;alskdjf;aslkdjf;asdjf;laskdjflsjdalfjd;alksjdfl;jsa;lfdja;slkdjf;alsjfd;lajsfl;dj";
        i2.width = 320;
        i2.height = 240;
        i2.size = Image.Size.SMALL;
        Image i3 = new Image();
        MediaContent mc = new MediaContent();
        mc.media = m;
        mc.images = new ArrayList<Image>(3);
        mc.images.add(i1);
        mc.images.add(i3);
        mc.images.add(i2);
        //@formatter:on
        return mc;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o)
            return true;
        if (o == null || getClass() != o.getClass()) {
            System.err.println("ah???");
            return false;
        }

        MediaContent that = (MediaContent) o;

        if (images != null ? !images.equals(that.images) : that.images != null) {
            System.err.println("images?");
            return false;
        }
        if (media != null ? !media.equals(that.media) : that.media != null) {
            System.err.println("media?");
            return false;
        }

        return true;
    }

    @Override
    public String toString() {
        String s = String.format("%s\n", getClass().toString());
        s += String.format("Media:\n%s", media == null ? "null" : media.toString());
        for (int i = 0; i < images.size(); i++) {
            Image img = images.get(i);
            s += String.format("Image[%d]:\n%s", i, img == null ? "null" : img.toString());
        }
        return s;
    }
}
