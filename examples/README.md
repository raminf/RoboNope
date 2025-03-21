# RoboNope Module Examples

This directory contains example files for use with the RoboNope NGINX module.

## Contents

### robots.txt

An example `robots.txt` file that demonstrates how to set up disallow rules for bots. The RoboNope module will parse this file to determine which URLs should be protected.

Example usage in NGINX configuration:
```nginx
robonope_robots_path /path/to/examples/robots.txt;
```

### static/

This directory contains static HTML content that can be served to bots that ignore the robots.txt rules. Instead of generating dynamic content, you can configure the module to serve these pre-made static pages.

Example usage in NGINX configuration:
```nginx
robonope_static_content_path /path/to/examples/static;
robonope_dynamic_content off;
```

#### Files in static/

- `index.html` - A general example page with honeypot links
- `secret.html` - An example of a disallowed page
- `styles.css` - Stylesheet for the example pages

## How to Use These Examples

1. Copy the examples to your desired location:
   ```bash
   cp -r examples /path/to/your/nginx/conf/
   ```

2. Update your NGINX configuration to point to these files:
   ```nginx
   robonope_enable on;
   robonope_robots_path /path/to/your/nginx/conf/examples/robots.txt;
   robonope_static_content_path /path/to/your/nginx/conf/examples/static;
   ```

3. Choose whether to use static or dynamic content:
   ```nginx
   # For static content
   robonope_dynamic_content off;
   
   # For dynamic content
   robonope_dynamic_content on;
   ```

4. Restart NGINX to apply the changes:
   ```bash
   nginx -s reload
   ```

## Customizing the Examples

Feel free to modify these examples to suit your needs:

- Update the `robots.txt` file to include your specific disallow rules
- Modify the static HTML files to match your website's design
- Add more static pages for different disallowed URLs
- Customize the CSS to match your branding

## Testing the Examples

You can test how these examples work with the RoboNope module by using the included demo:

```bash
make demo
```

This will start a temporary NGINX instance with the RoboNope module configured to use these examples. 